/*
 * Border Queue Optimizer
 * CLI entry point
 *
 * Usage:
 *   border-queue-optimizer simulate [--lanes N] [--duration S] [--rate R]
 *   border-queue-optimizer optimize --rate R --service-rate S [--max-lanes N]
 *   border-queue-optimizer predict  --hour H --minute M
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../include/common.h"
#include "simulator.h"
#include "optimizer.h"
#include "predictor.h"
#include "queue_model.h"

/* ── Defaults ────────────────────────────────────────────────────────── */

#define DEFAULT_LANES           4
#define DEFAULT_DURATION        28800.0   /* 8 hours */
#define DEFAULT_ARRIVAL_RATE    120.0     /* vehicles/hour */
#define DEFAULT_SERVICE_RATE    40.0      /* vehicles/hour/lane */
#define DEFAULT_RUSH_FACTOR     2.0
#define DEFAULT_RUSH_START      7
#define DEFAULT_RUSH_END        10

/* ── Usage ───────────────────────────────────────────────────────────── */

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Border Queue Optimizer v%d.%d.%d\n"
        "\n"
        "Usage:\n"
        "  %s simulate [options]\n"
        "  %s optimize [options]\n"
        "  %s predict  [options]\n"
        "\n"
        "Simulate options:\n"
        "  --lanes N          Number of inspection lanes (default: %d)\n"
        "  --duration S       Simulation duration in seconds (default: %.0f)\n"
        "  --rate R           Base arrival rate, vehicles/hour (default: %.0f)\n"
        "  --service-rate S   Service rate per lane, vehicles/hour (default: %.0f)\n"
        "  --seed N           RNG seed (default: %d)\n"
        "\n"
        "Optimize options:\n"
        "  --rate R           Arrival rate (required)\n"
        "  --service-rate S   Service rate per lane (required)\n"
        "  --max-lanes N      Maximum lanes available (default: 16)\n"
        "  --max-wait S       Maximum acceptable avg wait in seconds (default: 300)\n"
        "\n"
        "Predict options:\n"
        "  --hour H           Hour of day (0-23)\n"
        "  --minute M         Minute (0-59)\n"
        "\n",
        BQO_VERSION_MAJOR, BQO_VERSION_MINOR, BQO_VERSION_PATCH,
        prog, prog, prog,
        DEFAULT_LANES, DEFAULT_DURATION, DEFAULT_ARRIVAL_RATE,
        DEFAULT_SERVICE_RATE, BQO_DEFAULT_SEED);
}

/* ── Argument helpers ────────────────────────────────────────────────── */

static int find_arg(int argc, char **argv, const char *name)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], name) == 0)
            return i;
    }
    return -1;
}

static double get_arg_double(int argc, char **argv, const char *name,
                             double def)
{
    int idx = find_arg(argc, argv, name);
    if (idx >= 0 && idx + 1 < argc)
        return atof(argv[idx + 1]);
    return def;
}

static int get_arg_int(int argc, char **argv, const char *name, int def)
{
    int idx = find_arg(argc, argv, name);
    if (idx >= 0 && idx + 1 < argc)
        return atoi(argv[idx + 1]);
    return def;
}

/* ── Simulate ────────────────────────────────────────────────────────── */

static int cmd_simulate(int argc, char **argv)
{
    sim_config_t config = {
        .duration           = get_arg_double(argc, argv, "--duration", DEFAULT_DURATION),
        .warmup             = 600.0,
        .num_lanes          = get_arg_int(argc, argv, "--lanes", DEFAULT_LANES),
        .base_arrival_rate  = get_arg_double(argc, argv, "--rate", DEFAULT_ARRIVAL_RATE),
        .base_service_rate  = get_arg_double(argc, argv, "--service-rate", DEFAULT_SERVICE_RATE),
        .rush_hour_factor   = DEFAULT_RUSH_FACTOR,
        .rush_start_hour    = DEFAULT_RUSH_START,
        .rush_end_hour      = DEFAULT_RUSH_END,
        .max_queue_capacity = 0,
        .seed               = (uint64_t)get_arg_int(argc, argv, "--seed", BQO_DEFAULT_SEED),
    };

    printf("=== Border Queue Simulation ===\n");
    printf("Lanes:        %d\n", config.num_lanes);
    printf("Duration:     %.0f sec (%.1f hours)\n",
           config.duration, config.duration / 3600.0);
    printf("Arrival rate: %.1f vehicles/hour\n", config.base_arrival_rate);
    printf("Service rate: %.1f vehicles/hour/lane\n", config.base_service_rate);
    printf("Rush factor:  %.1fx (%d:00-%d:00)\n",
           config.rush_hour_factor, config.rush_start_hour, config.rush_end_hour);
    printf("\nRunning simulation...\n\n");

    simulator_t sim;
    bqo_error_t err = sim_init(&sim, &config);
    if (err != BQO_OK) {
        fprintf(stderr, "Error: failed to initialise simulator (code %d)\n", err);
        return 1;
    }

    sim_result_t result;
    err = sim_run(&sim, &result);
    sim_destroy(&sim);

    if (err != BQO_OK) {
        fprintf(stderr, "Error: simulation failed (code %d)\n", err);
        return 1;
    }

    printf("=== Results ===\n");
    printf("Total arrivals:     %d\n",   result.total_arrivals);
    printf("Total served:       %d\n",   result.total_served);
    printf("Total balked:       %d\n",   result.total_balked);
    printf("Avg wait time:      %.1f sec\n", result.avg_wait_time);
    printf("Max wait time:      %.1f sec\n", result.max_wait_time);
    printf("P95 wait time:      %.1f sec\n", result.p95_wait_time);
    printf("Avg queue length:   %.1f\n",  result.avg_queue_length);
    printf("Max queue length:   %.0f\n",  result.max_queue_length);
    printf("Utilization:        %.1f%%\n", result.utilization * 100.0);
    printf("Throughput:         %.1f vehicles/hour\n", result.throughput);

    return 0;
}

/* ── Optimize ────────────────────────────────────────────────────────── */

static int cmd_optimize(int argc, char **argv)
{
    double lambda    = get_arg_double(argc, argv, "--rate", DEFAULT_ARRIVAL_RATE);
    double mu        = get_arg_double(argc, argv, "--service-rate", DEFAULT_SERVICE_RATE);
    int    max_lanes = get_arg_int(argc, argv, "--max-lanes", 16);
    double max_wait  = get_arg_double(argc, argv, "--max-wait", 300.0);

    opt_constraints_t constraints = {
        .min_lanes          = 1,
        .max_lanes          = max_lanes,
        .max_avg_wait       = max_wait,
        .target_utilization = 0.9,
        .staff_cost_per_lane = 50.0,
        .wait_cost_per_sec  = 0.10,
    };

    printf("=== Lane Allocation Optimizer ===\n");
    printf("Arrival rate:  %.1f vehicles/hour\n", lambda);
    printf("Service rate:  %.1f vehicles/hour/lane\n", mu);
    printf("Max lanes:     %d\n", max_lanes);
    printf("Max avg wait:  %.0f sec\n", max_wait);
    printf("\n");

    /* Also show M/M/c analytical results for reference */
    int min_stable = opt_min_stable_lanes(lambda, mu);
    printf("Minimum lanes for stability: %d\n", min_stable);
    printf("\nAnalytical M/M/c results by lane count:\n");
    printf("  Lanes  Util%%   Avg Wait(s)  Erlang-C\n");
    for (int c = min_stable; c <= BQO_MIN(max_lanes, min_stable + 6); c++) {
        double rho = mmc_utilization(lambda, mu, c);
        double wq  = mmc_avg_wait_time(lambda, mu, c) * BQO_SECONDS_PER_HOUR;
        double ec  = mmc_erlang_c(lambda, mu, c);
        printf("  %5d  %5.1f%%  %11.1f  %8.4f\n",
               c, rho * 100.0, wq, ec);
    }

    opt_result_t result;
    bqo_error_t err = opt_find_optimal_lanes(lambda, mu, &constraints, &result);
    if (err != BQO_OK) {
        fprintf(stderr, "\nError: no feasible lane allocation found (code %d)\n", err);
        return 1;
    }

    printf("\n=== Optimal Allocation ===\n");
    printf("Optimal lanes:    %d\n",     result.optimal_lanes);
    printf("Expected wait:    %.1f sec\n", result.expected_avg_wait);
    printf("Utilization:      %.1f%%\n",  result.expected_utilization * 100.0);
    printf("Total cost/hour:  $%.2f\n",   result.total_cost);
    printf("Throughput:       %.1f vehicles/hour\n", result.throughput);

    return 0;
}

/* ── Predict ─────────────────────────────────────────────────────────── */

static int cmd_predict(int argc, char **argv)
{
    int hour   = get_arg_int(argc, argv, "--hour", 8);
    int minute = get_arg_int(argc, argv, "--minute", 0);

    printf("=== Wait Time Prediction ===\n");
    printf("Time: %02d:%02d\n\n", hour, minute);

    /* Create predictor and seed with synthetic historical data */
    predictor_t pred;
    bqo_error_t err = predictor_init(&pred, 0.3);
    if (err != BQO_OK) {
        fprintf(stderr, "Error: failed to initialise predictor\n");
        return 1;
    }

    /* Seed with a realistic daily pattern (seconds) */
    double historical[PREDICTOR_SLOTS];
    for (int slot = 0; slot < PREDICTOR_SLOTS; slot++) {
        double h = (double)slot / 4.0;
        /* Base wait + rush hour peak */
        double base = 60.0;
        if (h >= 7.0 && h <= 10.0)
            base = 60.0 + 240.0 * exp(-2.0 * pow((h - 8.5) / 1.5, 2));
        else if (h >= 16.0 && h <= 19.0)
            base = 60.0 + 180.0 * exp(-2.0 * pow((h - 17.5) / 1.5, 2));
        historical[slot] = base;
    }
    predictor_seed(&pred, historical);

    double lo, hi;
    double predicted = predictor_predict_ci(&pred, hour, minute, &lo, &hi);

    printf("Predicted wait:     %.1f sec (%.1f min)\n",
           predicted, predicted / 60.0);
    printf("95%% CI:             [%.1f, %.1f] sec\n", lo, hi);
    printf("                    [%.1f, %.1f] min\n", lo / 60.0, hi / 60.0);

    return 0;
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "simulate") == 0)
        return cmd_simulate(argc, argv);
    if (strcmp(cmd, "optimize") == 0)
        return cmd_optimize(argc, argv);
    if (strcmp(cmd, "predict") == 0)
        return cmd_predict(argc, argv);
    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    fprintf(stderr, "Unknown command: %s\n\n", cmd);
    print_usage(argv[0]);
    return 1;
}
