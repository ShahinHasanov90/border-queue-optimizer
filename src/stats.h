/*
 * Border Queue Optimizer
 * Online statistics collector: mean, variance, percentiles, utilization
 *
 * Uses Welford's online algorithm for numerically stable mean/variance.
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#ifndef BQO_STATS_H
#define BQO_STATS_H

#include "../include/common.h"

/* Online statistics accumulator */
typedef struct {
    int64_t count;
    double  mean;
    double  m2;         /* sum of squared deviations (Welford)          */
    double  min_val;
    double  max_val;
    double  sum;

    /* For approximate percentiles: fixed-size sorted reservoir          */
    double *reservoir;
    int     reservoir_cap;
    int     reservoir_size;
} stats_t;

/* Initialise a statistics collector.
 * reservoir_capacity controls percentile accuracy (0 = disable).       */
bqo_error_t stats_init(stats_t *st, int reservoir_capacity);

/* Free internal buffers.                                               */
void stats_destroy(stats_t *st);

/* Add an observation.                                                  */
void stats_observe(stats_t *st, double value);

/* Accessors */
double stats_mean(const stats_t *st);
double stats_variance(const stats_t *st);
double stats_stddev(const stats_t *st);
double stats_min(const stats_t *st);
double stats_max(const stats_t *st);
double stats_sum(const stats_t *st);
int64_t stats_count(const stats_t *st);

/* Return the p-th percentile (p in [0, 100]).
 * Requires reservoir to have been enabled.
 * Returns -1.0 if unavailable.                                         */
double stats_percentile(const stats_t *st, double p);

/* Reset all counters.                                                  */
void stats_reset(stats_t *st);

/* Compute utilization = busy_time / total_time.                        */
double stats_utilization(double busy_time, double total_time);

#endif /* BQO_STATS_H */
