/*
 * Border Queue Optimizer
 * Wait time predictor implementation
 *
 * Maintains per-slot (15-minute intervals) exponentially smoothed
 * wait-time estimates.  Simple exponential smoothing:
 *
 *   S_t = alpha * X_t + (1 - alpha) * S_{t-1}
 *
 * Confidence intervals are approximated as +/- 1.96 * stddev,
 * where stddev is estimated from the smoothing residuals.
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include "predictor.h"
#include <math.h>
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────────────── */

static int time_to_slot(int hour, int minute)
{
    int slot = hour * 4 + minute / 15;
    if (slot < 0) slot = 0;
    if (slot >= PREDICTOR_SLOTS) slot = PREDICTOR_SLOTS - 1;
    return slot;
}

/* ── Public API ──────────────────────────────────────────────────────── */

bqo_error_t predictor_init(predictor_t *pred, double alpha)
{
    if (!pred)
        return BQO_ERR_INVALID;
    if (alpha <= 0.0 || alpha >= 1.0)
        return BQO_ERR_INVALID;

    memset(pred, 0, sizeof(*pred));
    pred->alpha       = alpha;
    pred->initialized = true;
    return BQO_OK;
}

bqo_error_t predictor_observe(predictor_t *pred,
                              int hour, int minute, double wait_time)
{
    if (!pred || !pred->initialized)
        return BQO_ERR_INVALID;
    if (hour < 0 || hour >= 24 || minute < 0 || minute >= 60)
        return BQO_ERR_INVALID;

    int slot = time_to_slot(hour, minute);

    if (pred->counts[slot] == 0) {
        /* First observation: initialise with the raw value */
        pred->smoothed[slot] = wait_time;
    } else {
        /* Exponential smoothing update */
        pred->smoothed[slot] = pred->alpha * wait_time +
                               (1.0 - pred->alpha) * pred->smoothed[slot];
    }
    pred->counts[slot]++;

    return BQO_OK;
}

double predictor_predict(const predictor_t *pred, int hour, int minute)
{
    if (!pred || !pred->initialized)
        return 0.0;

    int slot = time_to_slot(hour, minute);

    if (pred->counts[slot] > 0)
        return pred->smoothed[slot];

    /* No data for this slot: interpolate from neighbours */
    int prev = (slot - 1 + PREDICTOR_SLOTS) % PREDICTOR_SLOTS;
    int next = (slot + 1) % PREDICTOR_SLOTS;

    double sum = 0.0;
    int    n   = 0;
    if (pred->counts[prev] > 0) { sum += pred->smoothed[prev]; n++; }
    if (pred->counts[next] > 0) { sum += pred->smoothed[next]; n++; }

    return (n > 0) ? sum / (double)n : 0.0;
}

double predictor_predict_ci(const predictor_t *pred,
                            int hour, int minute,
                            double *lo, double *hi)
{
    double mean = predictor_predict(pred, hour, minute);

    /* Simple heuristic CI: wider when fewer observations */
    int slot = time_to_slot(hour, minute);
    int count = (pred && pred->initialized) ? pred->counts[slot] : 0;

    double half_width;
    if (count < 2) {
        half_width = mean * 0.5;  /* 50% uncertainty */
    } else {
        /* Approximate: shrink CI as observations grow */
        half_width = mean * 1.96 / sqrt((double)count);
    }

    if (lo) *lo = BQO_MAX(0.0, mean - half_width);
    if (hi) *hi = mean + half_width;

    return mean;
}

bqo_error_t predictor_seed(predictor_t *pred, const double *historical)
{
    if (!pred || !pred->initialized || !historical)
        return BQO_ERR_INVALID;

    for (int i = 0; i < PREDICTOR_SLOTS; i++) {
        pred->smoothed[i] = historical[i];
        pred->counts[i]   = 1;
    }
    return BQO_OK;
}

void predictor_reset(predictor_t *pred)
{
    if (!pred)
        return;
    memset(pred->smoothed, 0, sizeof(pred->smoothed));
    memset(pred->counts,   0, sizeof(pred->counts));
}
