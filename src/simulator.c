/*
 * Border Queue Optimizer
 * Discrete event simulator implementation
 *
 * Simulates vehicle arrivals at a customs border crossing with multiple
 * inspection lanes.  Events (arrivals and departures) are scheduled on
 * a min-heap ordered by timestamp.
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include "simulator.h"
#include "arrival.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── xorshift64 PRNG ────────────────────────────────────────────────── */

static uint64_t xorshift64(uint64_t *state)
{
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

double sim_rand_uniform(simulator_t *sim)
{
    return (double)(xorshift64(&sim->rng_state) >> 11) / (double)(1ULL << 53);
}

double sim_rand_exponential(simulator_t *sim, double mean)
{
    double u = sim_rand_uniform(sim);
    while (u < BQO_EPSILON)
        u = sim_rand_uniform(sim);
    return -mean * log(u);
}

/* ── Initialisation ──────────────────────────────────────────────────── */

bqo_error_t sim_init(simulator_t *sim, const sim_config_t *config)
{
    if (!sim || !config)
        return BQO_ERR_INVALID;
    if (config->num_lanes <= 0 || config->num_lanes > BQO_MAX_LANES)
        return BQO_ERR_INVALID;

    memset(sim, 0, sizeof(*sim));
    sim->config = *config;
    sim->clock  = 0.0;
    sim->rng_state = config->seed ? config->seed : BQO_DEFAULT_SEED;

    bqo_error_t err = heap_create(&sim->event_queue, 1024);
    if (err != BQO_OK)
        return err;

    err = stats_init(&sim->wait_stats, BQO_PERCENTILE_SLOTS);
    if (err != BQO_OK) {
        heap_destroy(&sim->event_queue);
        return err;
    }

    err = stats_init(&sim->queue_stats, 0);
    if (err != BQO_OK) {
        stats_destroy(&sim->wait_stats);
        heap_destroy(&sim->event_queue);
        return err;
    }

    /* Initialise lanes */
    for (int i = 0; i < config->num_lanes; i++) {
        sim->lanes[i].id            = i;
        sim->lanes[i].active        = true;
        sim->lanes[i].queue_length  = 0;
        sim->lanes[i].service_rate  = config->base_service_rate;
        sim->lanes[i].total_busy_time     = 0.0;
        sim->lanes[i].vehicles_processed  = 0;
    }

    return BQO_OK;
}

void sim_destroy(simulator_t *sim)
{
    if (sim) {
        heap_destroy(&sim->event_queue);
        stats_destroy(&sim->wait_stats);
        stats_destroy(&sim->queue_stats);
    }
}

/* ── Lane selection ──────────────────────────────────────────────────── */

int sim_shortest_queue_lane(const simulator_t *sim)
{
    int best = -1;
    int best_len = INT32_MAX;
    for (int i = 0; i < sim->config.num_lanes; i++) {
        if (!sim->lanes[i].active)
            continue;
        if (sim->lanes[i].queue_length < best_len) {
            best_len = sim->lanes[i].queue_length;
            best = i;
        }
    }
    return best;
}

/* ── Event scheduling ────────────────────────────────────────────────── */

bqo_error_t sim_schedule_arrival(simulator_t *sim, double time)
{
    event_t ev = {
        .time       = time,
        .type       = EVENT_ARRIVAL,
        .lane_id    = -1,
        .vehicle_id = sim->next_vehicle_id++,
        .vtype      = VEHICLE_CAR
    };
    return heap_push(&sim->event_queue, ev);
}

static bqo_error_t schedule_departure(simulator_t *sim, int lane_id,
                                       int vehicle_id, double time)
{
    event_t ev = {
        .time       = time,
        .type       = EVENT_DEPARTURE,
        .lane_id    = lane_id,
        .vehicle_id = vehicle_id,
        .vtype      = VEHICLE_CAR
    };
    return heap_push(&sim->event_queue, ev);
}

/* ── Arrival rate as a function of time ──────────────────────────────── */

static double arrival_rate(const simulator_t *sim, double time)
{
    double hour = fmod(time / BQO_SECONDS_PER_HOUR, 24.0);
    double rate = sim->config.base_arrival_rate;

    int h = (int)hour;
    if (h >= sim->config.rush_start_hour && h < sim->config.rush_end_hour)
        rate *= sim->config.rush_hour_factor;

    return rate;
}

/* ── Event processing ────────────────────────────────────────────────── */

bqo_error_t sim_process_event(simulator_t *sim, const event_t *ev)
{
    sim->clock = ev->time;

    switch (ev->type) {
    case EVENT_ARRIVAL: {
        sim->total_arrivals++;

        int lane = sim_shortest_queue_lane(sim);
        if (lane < 0)
            return BQO_OK;  /* no active lanes */

        /* Check capacity */
        if (sim->config.max_queue_capacity > 0 &&
            sim->lanes[lane].queue_length >= sim->config.max_queue_capacity) {
            sim->total_balked++;
            break;
        }

        sim->lanes[lane].queue_length++;
        stats_observe(&sim->queue_stats, (double)sim->lanes[lane].queue_length);

        /* If the lane was idle, start service immediately */
        if (sim->lanes[lane].queue_length == 1) {
            double service_time = sim_rand_exponential(
                sim, BQO_SECONDS_PER_HOUR / sim->lanes[lane].service_rate);
            schedule_departure(sim, lane, ev->vehicle_id,
                               sim->clock + service_time);
        } else {
            /* Vehicle must wait; wait time will be recorded at departure */
        }

        /* Schedule next arrival */
        double rate = arrival_rate(sim, sim->clock);
        double inter = sim_rand_exponential(sim, BQO_SECONDS_PER_HOUR / rate);
        if (sim->clock + inter < sim->config.duration) {
            sim_schedule_arrival(sim, sim->clock + inter);
        }
        break;
    }

    case EVENT_DEPARTURE: {
        int lane = ev->lane_id;
        if (lane < 0 || lane >= sim->config.num_lanes)
            return BQO_ERR_INVALID;

        sim->lanes[lane].queue_length--;
        sim->lanes[lane].vehicles_processed++;
        sim->total_served++;

        double service_time = BQO_SECONDS_PER_HOUR / sim->lanes[lane].service_rate;
        sim->lanes[lane].total_busy_time += service_time;

        /* Record wait time (approximate: queue_length * avg service time) */
        double wait = (double)sim->lanes[lane].queue_length * service_time;
        if (sim->clock > sim->config.warmup) {
            stats_observe(&sim->wait_stats, wait);
        }

        /* If more vehicles in queue, start serving next */
        if (sim->lanes[lane].queue_length > 0) {
            double next_service = sim_rand_exponential(
                sim, BQO_SECONDS_PER_HOUR / sim->lanes[lane].service_rate);
            schedule_departure(sim, lane, -1,
                               sim->clock + next_service);
        }
        break;
    }

    default:
        break;
    }

    return BQO_OK;
}

/* ── Main simulation loop ────────────────────────────────────────────── */

bqo_error_t sim_run(simulator_t *sim, sim_result_t *result)
{
    if (!sim || !result)
        return BQO_ERR_INVALID;

    /* Schedule first arrival */
    double rate = arrival_rate(sim, 0.0);
    double first = sim_rand_exponential(sim, BQO_SECONDS_PER_HOUR / rate);
    bqo_error_t err = sim_schedule_arrival(sim, first);
    if (err != BQO_OK)
        return err;

    /* Process events until the simulation duration is reached */
    event_t ev;
    while (heap_pop(&sim->event_queue, &ev) == BQO_OK) {
        if (ev.time > sim->config.duration)
            break;
        err = sim_process_event(sim, &ev);
        if (err != BQO_OK)
            return err;
    }

    /* Collect results */
    memset(result, 0, sizeof(*result));
    result->avg_wait_time    = stats_mean(&sim->wait_stats);
    result->max_wait_time    = stats_max(&sim->wait_stats);
    result->p95_wait_time    = stats_percentile(&sim->wait_stats, 95.0);
    result->avg_queue_length = stats_mean(&sim->queue_stats);
    result->max_queue_length = stats_max(&sim->queue_stats);
    result->total_arrivals   = sim->total_arrivals;
    result->total_served     = sim->total_served;
    result->total_balked     = sim->total_balked;

    /* Compute average utilization across lanes */
    double total_util = 0.0;
    for (int i = 0; i < sim->config.num_lanes; i++) {
        total_util += stats_utilization(sim->lanes[i].total_busy_time,
                                         sim->config.duration);
    }
    result->utilization = total_util / (double)sim->config.num_lanes;
    result->throughput  = (double)sim->total_served /
                          (sim->config.duration / BQO_SECONDS_PER_HOUR);

    return BQO_OK;
}
