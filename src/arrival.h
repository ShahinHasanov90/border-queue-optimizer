/*
 * Border Queue Optimizer
 * Arrival pattern generator: Poisson process with time-varying rate
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#ifndef BQO_ARRIVAL_H
#define BQO_ARRIVAL_H

#include "../include/common.h"

/* Arrival pattern configuration */
typedef struct {
    double  base_rate;          /* base arrival rate (vehicles/hour)    */
    double  rush_factor;        /* multiplier during rush hours         */
    int     rush_start;         /* rush start hour (0-23)              */
    int     rush_end;           /* rush end hour (0-23)                */
    double  seasonal_factor;    /* seasonal adjustment (1.0 = normal)  */
    double  weekend_factor;     /* weekend adjustment (typically < 1)  */
    bool    is_weekend;
} arrival_config_t;

/* Arrival generator state (thinning algorithm for NHPP) */
typedef struct {
    arrival_config_t config;
    uint64_t         rng_state;
} arrival_gen_t;

/* Initialise the generator.                                            */
bqo_error_t arrival_init(arrival_gen_t *gen,
                         const arrival_config_t *config,
                         uint64_t seed);

/* Return the instantaneous arrival rate at the given simulation time
 * (seconds from midnight).                                             */
double arrival_rate_at(const arrival_gen_t *gen, double time);

/* Generate the next inter-arrival time starting from `current_time`.
 * Uses Lewis-Shedler thinning for the non-homogeneous Poisson process. */
double arrival_next(arrival_gen_t *gen, double current_time);

/* Compute the peak (maximum) arrival rate over the entire day.         */
double arrival_peak_rate(const arrival_gen_t *gen);

/* Fill an array with hourly arrival rates (24 entries).                */
void arrival_hourly_rates(const arrival_gen_t *gen, double *rates);

#endif /* BQO_ARRIVAL_H */
