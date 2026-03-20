/*
 * Border Queue Optimizer
 * Arrival pattern generator implementation
 *
 * Generates inter-arrival times from a non-homogeneous Poisson process
 * (NHPP) with time-varying rate lambda(t).  Uses the Lewis-Shedler
 * thinning algorithm:
 *
 *   1. Find lambda_max = max lambda(t) over the day.
 *   2. Generate candidate arrival from HPP(lambda_max).
 *   3. Accept with probability lambda(t) / lambda_max.
 *   4. If rejected, repeat from step 2.
 *
 * The rate function models rush-hour peaks (configurable start/end),
 * seasonal adjustments, and weekend effects.
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include "arrival.h"
#include <math.h>
#include <string.h>

/* ── xorshift64 (local copy to keep arrival module self-contained) ─── */

static uint64_t xorshift64(uint64_t *state)
{
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static double rand_uniform(uint64_t *state)
{
    return (double)(xorshift64(state) >> 11) / (double)(1ULL << 53);
}

static double rand_exponential(uint64_t *state, double mean)
{
    double u = rand_uniform(state);
    while (u < BQO_EPSILON)
        u = rand_uniform(state);
    return -mean * log(u);
}

/* ── Public API ──────────────────────────────────────────────────────── */

bqo_error_t arrival_init(arrival_gen_t *gen,
                         const arrival_config_t *config,
                         uint64_t seed)
{
    if (!gen || !config)
        return BQO_ERR_INVALID;

    gen->config    = *config;
    gen->rng_state = seed ? seed : BQO_DEFAULT_SEED;
    return BQO_OK;
}

double arrival_rate_at(const arrival_gen_t *gen, double time)
{
    if (!gen)
        return 0.0;

    /* Convert simulation time (seconds) to hour of day */
    double hour = fmod(time / BQO_SECONDS_PER_HOUR, 24.0);
    int h = (int)hour;

    double rate = gen->config.base_rate;

    /* Rush-hour boost */
    if (h >= gen->config.rush_start && h < gen->config.rush_end) {
        /* Smooth bell curve within rush period */
        double mid = (gen->config.rush_start + gen->config.rush_end) / 2.0;
        double half_width = (gen->config.rush_end - gen->config.rush_start) / 2.0;
        double x = (hour - mid) / half_width;  /* normalised to [-1, 1] */
        double bell = exp(-2.0 * x * x);       /* Gaussian-like shape */
        rate *= 1.0 + (gen->config.rush_factor - 1.0) * bell;
    }

    /* Seasonal and weekend adjustments */
    rate *= gen->config.seasonal_factor;
    if (gen->config.is_weekend)
        rate *= gen->config.weekend_factor;

    return rate;
}

double arrival_peak_rate(const arrival_gen_t *gen)
{
    double peak = 0.0;
    /* Sample every 15 minutes */
    for (int slot = 0; slot < 96; slot++) {
        double t = (double)slot * 900.0;  /* 900 sec = 15 min */
        double r = arrival_rate_at(gen, t);
        if (r > peak) peak = r;
    }
    return peak;
}

double arrival_next(arrival_gen_t *gen, double current_time)
{
    if (!gen)
        return 0.0;

    double lambda_max = arrival_peak_rate(gen);
    if (lambda_max <= BQO_EPSILON)
        return INFINITY;

    /* Lewis-Shedler thinning */
    double t = current_time;
    while (1) {
        double inter = rand_exponential(&gen->rng_state,
                                         BQO_SECONDS_PER_HOUR / lambda_max);
        t += inter;

        double lambda_t = arrival_rate_at(gen, t);
        double u = rand_uniform(&gen->rng_state);

        if (u <= lambda_t / lambda_max)
            return t - current_time;  /* accepted: return inter-arrival time */
    }
}

void arrival_hourly_rates(const arrival_gen_t *gen, double *rates)
{
    if (!gen || !rates)
        return;
    for (int h = 0; h < 24; h++) {
        /* Rate at the middle of each hour */
        double t = ((double)h + 0.5) * BQO_SECONDS_PER_HOUR;
        rates[h] = arrival_rate_at(gen, t);
    }
}
