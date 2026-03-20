/*
 * Border Queue Optimizer
 * Wait time predictor using exponential smoothing and time-of-day patterns
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#ifndef BQO_PREDICTOR_H
#define BQO_PREDICTOR_H

#include "../include/common.h"

#define PREDICTOR_HOURS  24
#define PREDICTOR_SLOTS  (PREDICTOR_HOURS * 4)  /* 15-min slots */

/* Predictor state: maintains per-slot smoothed estimates */
typedef struct {
    double  smoothed[PREDICTOR_SLOTS];   /* smoothed wait time per slot */
    int     counts[PREDICTOR_SLOTS];     /* number of observations      */
    double  alpha;                       /* smoothing factor (0,1)      */
    bool    initialized;
} predictor_t;

/* Initialise predictor with a smoothing factor alpha in (0, 1).
 * Typical value: 0.3                                                   */
bqo_error_t predictor_init(predictor_t *pred, double alpha);

/* Feed an observed wait time for the given hour and minute.            */
bqo_error_t predictor_observe(predictor_t *pred,
                              int hour, int minute, double wait_time);

/* Predict expected wait time for the given time of day.                */
double predictor_predict(const predictor_t *pred, int hour, int minute);

/* Predict with confidence interval (returns mean; lo/hi set to bounds).*/
double predictor_predict_ci(const predictor_t *pred,
                            int hour, int minute,
                            double *lo, double *hi);

/* Seed predictor with historical averages (array of PREDICTOR_SLOTS).  */
bqo_error_t predictor_seed(predictor_t *pred, const double *historical);

/* Reset all slots.                                                     */
void predictor_reset(predictor_t *pred);

#endif /* BQO_PREDICTOR_H */
