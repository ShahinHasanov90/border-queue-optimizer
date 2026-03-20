/*
 * Border Queue Optimizer
 * Lane allocation optimizer implementation
 *
 * Exhaustive search over feasible lane counts [min_lanes, max_lanes].
 * For each candidate c, computes M/M/c metrics and total cost
 * (staffing + waiting).  Selects the c that minimizes total cost
 * while respecting the wait-time and utilization constraints.
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include "optimizer.h"
#include "queue_model.h"
#include <math.h>

/* ── Minimum stable lanes ────────────────────────────────────────────── */

int opt_min_stable_lanes(double lambda, double mu)
{
    if (mu <= BQO_EPSILON)
        return -1;
    /* Need c such that lambda / (c * mu) < 1, i.e. c > lambda / mu */
    int c = (int)ceil(lambda / mu);
    if ((double)c * mu <= lambda)
        c++;
    return c;
}

/* ── Total hourly cost ───────────────────────────────────────────────── */

double opt_total_cost(double lambda, double mu, int lanes,
                      double staff_cost, double wait_cost)
{
    double rho = mmc_utilization(lambda, mu, lanes);
    if (rho >= 1.0)
        return INFINITY;

    /* Staffing cost: per-lane-hour * number of lanes */
    double staffing = staff_cost * (double)lanes;

    /* Waiting cost: avg_wait * arrival_rate * cost_per_second_of_wait
     * This gives expected total waiting cost per hour.                  */
    double avg_wait = mmc_avg_wait_time(lambda, mu, lanes);
    double waiting  = avg_wait * lambda * wait_cost;

    return staffing + waiting;
}

/* ── Optimiser ───────────────────────────────────────────────────────── */

bqo_error_t opt_find_optimal_lanes(double lambda, double mu,
                                   const opt_constraints_t *constraints,
                                   opt_result_t *result)
{
    if (!constraints || !result)
        return BQO_ERR_INVALID;
    if (lambda <= 0.0 || mu <= 0.0)
        return BQO_ERR_INVALID;

    int c_min = constraints->min_lanes;
    int c_max = constraints->max_lanes;

    /* Ensure minimum stability */
    int c_stable = opt_min_stable_lanes(lambda, mu);
    if (c_min < c_stable)
        c_min = c_stable;

    if (c_min > c_max)
        return BQO_ERR_INVALID;  /* no feasible solution */

    double best_cost = INFINITY;
    int    best_c    = -1;

    for (int c = c_min; c <= c_max; c++) {
        double rho = mmc_utilization(lambda, mu, c);
        if (rho >= 1.0)
            continue;

        /* Check utilization constraint */
        if (constraints->target_utilization > 0.0 &&
            rho > constraints->target_utilization)
            continue;

        double avg_wait = mmc_avg_wait_time(lambda, mu, c);

        /* Check wait-time constraint (convert hours to seconds) */
        double avg_wait_sec = avg_wait * BQO_SECONDS_PER_HOUR;
        if (constraints->max_avg_wait > 0.0 &&
            avg_wait_sec > constraints->max_avg_wait)
            continue;

        double cost = opt_total_cost(lambda, mu, c,
                                     constraints->staff_cost_per_lane,
                                     constraints->wait_cost_per_sec);

        if (cost < best_cost) {
            best_cost = cost;
            best_c    = c;
        }
    }

    if (best_c < 0)
        return BQO_ERR_INVALID;  /* no feasible solution found */

    result->optimal_lanes       = best_c;
    result->expected_avg_wait   = mmc_avg_wait_time(lambda, mu, best_c) *
                                  BQO_SECONDS_PER_HOUR;
    result->expected_utilization = mmc_utilization(lambda, mu, best_c);
    result->total_cost          = best_cost;
    result->throughput          = lambda;  /* in steady state */

    return BQO_OK;
}
