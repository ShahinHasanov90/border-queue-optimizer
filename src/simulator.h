/*
 * Border Queue Optimizer
 * Discrete event simulator for border crossing queues
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#ifndef BQO_SIMULATOR_H
#define BQO_SIMULATOR_H

#include "../include/common.h"
#include "heap.h"
#include "stats.h"

/* Simulator state */
typedef struct {
    sim_config_t  config;
    min_heap_t    event_queue;
    lane_state_t  lanes[BQO_MAX_LANES];
    double        clock;            /* current simulation time          */
    int           next_vehicle_id;
    int           total_arrivals;
    int           total_served;
    int           total_balked;
    stats_t       wait_stats;       /* wait-time statistics             */
    stats_t       queue_stats;      /* queue-length statistics          */
    uint64_t      rng_state;        /* xorshift64 state                 */
} simulator_t;

/* Initialise the simulator with the given configuration.
 * Returns BQO_OK or an error code.                                     */
bqo_error_t sim_init(simulator_t *sim, const sim_config_t *config);

/* Run the full simulation.  Results are written to *result.            */
bqo_error_t sim_run(simulator_t *sim, sim_result_t *result);

/* Free resources held by the simulator.                                */
void sim_destroy(simulator_t *sim);

/* ── Internal helpers (exposed for testing) ────────────────────────── */

/* Generate a uniform random double in [0, 1).                          */
double sim_rand_uniform(simulator_t *sim);

/* Generate an exponential random variate with the given mean.          */
double sim_rand_exponential(simulator_t *sim, double mean);

/* Schedule a new arrival event.                                        */
bqo_error_t sim_schedule_arrival(simulator_t *sim, double time);

/* Process one event.                                                   */
bqo_error_t sim_process_event(simulator_t *sim, const event_t *ev);

/* Find the lane with the shortest queue.  Returns lane index or -1.    */
int sim_shortest_queue_lane(const simulator_t *sim);

#endif /* BQO_SIMULATOR_H */
