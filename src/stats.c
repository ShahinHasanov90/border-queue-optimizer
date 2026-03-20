/*
 * Border Queue Optimizer
 * Online statistics collector implementation
 *
 * Uses Welford's algorithm for numerically stable running mean/variance.
 * Approximate percentiles via a sorted reservoir (insertion sort on a
 * fixed-capacity array; trades memory for simplicity).
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include "stats.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ── Initialisation / teardown ───────────────────────────────────────── */

bqo_error_t stats_init(stats_t *st, int reservoir_capacity)
{
    if (!st)
        return BQO_ERR_INVALID;

    memset(st, 0, sizeof(*st));
    st->min_val = DBL_MAX;
    st->max_val = -DBL_MAX;

    if (reservoir_capacity > 0) {
        st->reservoir = malloc((size_t)reservoir_capacity * sizeof(double));
        if (!st->reservoir)
            return BQO_ERR_NOMEM;
        st->reservoir_cap = reservoir_capacity;
    }
    return BQO_OK;
}

void stats_destroy(stats_t *st)
{
    if (st) {
        free(st->reservoir);
        st->reservoir = NULL;
    }
}

/* ── Observation ─────────────────────────────────────────────────────── */

void stats_observe(stats_t *st, double value)
{
    if (!st)
        return;

    st->count++;
    st->sum += value;

    if (value < st->min_val) st->min_val = value;
    if (value > st->max_val) st->max_val = value;

    /* Welford's online algorithm */
    double delta  = value - st->mean;
    st->mean     += delta / (double)st->count;
    double delta2 = value - st->mean;
    st->m2       += delta * delta2;

    /* Reservoir: keep a sorted sample for percentile queries.
     * If the reservoir is not full, insert in sorted position.
     * Otherwise, replace a random element with reservoir sampling. */
    if (st->reservoir && st->reservoir_cap > 0) {
        if (st->reservoir_size < st->reservoir_cap) {
            /* Insert in sorted order */
            int pos = st->reservoir_size;
            while (pos > 0 && st->reservoir[pos - 1] > value) {
                st->reservoir[pos] = st->reservoir[pos - 1];
                pos--;
            }
            st->reservoir[pos] = value;
            st->reservoir_size++;
        } else {
            /* Simple reservoir sampling: replace element at random index.
             * For approximate percentiles this is acceptable.            */
            int idx = (int)(st->count % st->reservoir_cap);
            st->reservoir[idx] = value;

            /* Re-sort (insertion sort is fine for small reservoirs) */
            for (int i = 1; i < st->reservoir_size; i++) {
                double key = st->reservoir[i];
                int j = i - 1;
                while (j >= 0 && st->reservoir[j] > key) {
                    st->reservoir[j + 1] = st->reservoir[j];
                    j--;
                }
                st->reservoir[j + 1] = key;
            }
        }
    }
}

/* ── Accessors ───────────────────────────────────────────────────────── */

double stats_mean(const stats_t *st)
{
    return (st && st->count > 0) ? st->mean : 0.0;
}

double stats_variance(const stats_t *st)
{
    if (!st || st->count < 2)
        return 0.0;
    return st->m2 / (double)(st->count - 1);  /* sample variance */
}

double stats_stddev(const stats_t *st)
{
    return sqrt(stats_variance(st));
}

double stats_min(const stats_t *st)
{
    return (st && st->count > 0) ? st->min_val : 0.0;
}

double stats_max(const stats_t *st)
{
    return (st && st->count > 0) ? st->max_val : 0.0;
}

double stats_sum(const stats_t *st)
{
    return st ? st->sum : 0.0;
}

int64_t stats_count(const stats_t *st)
{
    return st ? st->count : 0;
}

double stats_percentile(const stats_t *st, double p)
{
    if (!st || !st->reservoir || st->reservoir_size == 0)
        return -1.0;
    if (p < 0.0) p = 0.0;
    if (p > 100.0) p = 100.0;

    double rank = (p / 100.0) * (double)(st->reservoir_size - 1);
    int lo = (int)rank;
    int hi = lo + 1;
    if (hi >= st->reservoir_size)
        hi = st->reservoir_size - 1;

    double frac = rank - (double)lo;
    return st->reservoir[lo] * (1.0 - frac) + st->reservoir[hi] * frac;
}

void stats_reset(stats_t *st)
{
    if (!st)
        return;
    st->count   = 0;
    st->mean    = 0.0;
    st->m2      = 0.0;
    st->min_val = DBL_MAX;
    st->max_val = -DBL_MAX;
    st->sum     = 0.0;
    st->reservoir_size = 0;
}

double stats_utilization(double busy_time, double total_time)
{
    if (total_time <= BQO_EPSILON)
        return 0.0;
    return busy_time / total_time;
}
