/*
 * Border Queue Optimizer
 * Lane allocation optimizer
 *
 * Given arrival rate and per-lane service rate, determines the optimal
 * number of lanes to open in order to minimize average wait time while
 * satisfying staffing and capacity constraints.
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#ifndef BQO_OPTIMIZER_H
#define BQO_OPTIMIZER_H

#include "../include/common.h"

/* Optimisation constraints */
typedef struct {
    int     min_lanes;          /* minimum lanes that must be open      */
    int     max_lanes;          /* maximum available lanes              */
    double  max_avg_wait;       /* maximum acceptable avg wait (sec)    */
    double  target_utilization; /* desired server utilization (0-1)     */
    double  staff_cost_per_lane;/* cost per lane per hour               */
    double  wait_cost_per_sec;  /* cost of one second of customer wait  */
} opt_constraints_t;

/* Optimisation result */
typedef struct {
    int     optimal_lanes;
    double  expected_avg_wait;  /* seconds                              */
    double  expected_utilization;
    double  total_cost;         /* staffing + waiting cost per hour     */
    double  throughput;         /* vehicles per hour                    */
} opt_result_t;

/* Find the number of lanes that minimizes total cost (staffing + waiting)
 * subject to the given constraints.
 *
 * lambda : arrival rate  (vehicles/hour)
 * mu     : service rate per lane (vehicles/hour)
 *
 * Returns BQO_OK on success, BQO_ERR_INVALID if no feasible solution. */
bqo_error_t opt_find_optimal_lanes(double lambda, double mu,
                                   const opt_constraints_t *constraints,
                                   opt_result_t *result);

/* Quick helper: minimum lanes needed for a stable system (rho < 1).    */
int opt_min_stable_lanes(double lambda, double mu);

/* Compute total hourly cost for a given lane count.                    */
double opt_total_cost(double lambda, double mu, int lanes,
                      double staff_cost, double wait_cost);

#endif /* BQO_OPTIMIZER_H */
