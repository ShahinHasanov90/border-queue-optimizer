/*
 * Border Queue Optimizer
 * Common types, constants, and error codes
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#ifndef BQO_COMMON_H
#define BQO_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <float.h>

/* ── Version ─────────────────────────────────────────────────────────── */
#define BQO_VERSION_MAJOR 1
#define BQO_VERSION_MINOR 0
#define BQO_VERSION_PATCH 0

/* ── Error codes ─────────────────────────────────────────────────────── */
typedef enum {
    BQO_OK              =  0,
    BQO_ERR_NOMEM       = -1,
    BQO_ERR_INVALID     = -2,
    BQO_ERR_OVERFLOW    = -3,
    BQO_ERR_EMPTY       = -4,
    BQO_ERR_IO          = -5,
    BQO_ERR_CONFIG      = -6,
    BQO_ERR_UNSTABLE    = -7   /* queue system is unstable (rho >= 1) */
} bqo_error_t;

/* ── Event types ─────────────────────────────────────────────────────── */
typedef enum {
    EVENT_ARRIVAL       = 0,
    EVENT_DEPARTURE     = 1,
    EVENT_LANE_OPEN     = 2,
    EVENT_LANE_CLOSE    = 3,
    EVENT_SHIFT_CHANGE  = 4
} event_type_t;

/* ── Vehicle categories ──────────────────────────────────────────────── */
typedef enum {
    VEHICLE_CAR         = 0,
    VEHICLE_TRUCK       = 1,
    VEHICLE_BUS         = 2,
    VEHICLE_DIPLOMATIC  = 3,
    VEHICLE_PRIORITY    = 4
} vehicle_type_t;

/* ── Core data structures ────────────────────────────────────────────── */

/* Simulation event */
typedef struct {
    double        time;         /* event timestamp (seconds)            */
    event_type_t  type;         /* type of event                        */
    int           lane_id;      /* associated lane (-1 if none)         */
    int           vehicle_id;   /* associated vehicle (-1 if none)      */
    vehicle_type_t vtype;       /* vehicle category                     */
} event_t;

/* Per-lane state */
typedef struct {
    int     id;
    bool    active;
    int     queue_length;
    double  service_rate;       /* vehicles per hour                    */
    double  total_busy_time;
    int     vehicles_processed;
} lane_state_t;

/* Simulation configuration */
typedef struct {
    double  duration;           /* total simulation time (seconds)      */
    double  warmup;             /* warmup period (seconds)              */
    int     num_lanes;          /* number of inspection lanes           */
    double  base_arrival_rate;  /* mean arrivals per hour               */
    double  base_service_rate;  /* mean service rate per lane per hour  */
    double  rush_hour_factor;   /* multiplier during peak hours         */
    int     rush_start_hour;    /* start of rush period (0-23)          */
    int     rush_end_hour;      /* end of rush period (0-23)            */
    int     max_queue_capacity; /* max vehicles in queue (0 = infinite) */
    uint64_t seed;              /* RNG seed                             */
} sim_config_t;

/* Simulation result summary */
typedef struct {
    double  avg_wait_time;
    double  max_wait_time;
    double  p95_wait_time;
    double  avg_queue_length;
    double  max_queue_length;
    double  utilization;
    int     total_arrivals;
    int     total_served;
    int     total_balked;       /* turned away due to full queue        */
    double  throughput;         /* vehicles per hour                    */
} sim_result_t;

/* ── Constants ───────────────────────────────────────────────────────── */
#define BQO_MAX_LANES           64
#define BQO_MAX_EVENTS          1000000
#define BQO_DEFAULT_SEED        42
#define BQO_SECONDS_PER_HOUR    3600.0
#define BQO_EPSILON             1e-12
#define BQO_PERCENTILE_SLOTS    10000

/* ── Utility macros ──────────────────────────────────────────────────── */
#define BQO_MIN(a, b)  ((a) < (b) ? (a) : (b))
#define BQO_MAX(a, b)  ((a) > (b) ? (a) : (b))

#endif /* BQO_COMMON_H */
