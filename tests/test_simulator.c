/*
 * Border Queue Optimizer
 * Tests for discrete event simulator
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/simulator.h"

/* ── Test framework (shared globals from test_main.c) ────────────────── */

extern int g_tests_run;
extern int g_tests_passed;
extern int g_tests_failed;

#define TEST(name) static void name(void)

#define RUN_TEST(name) do { \
    g_tests_run++; \
    printf("  %-50s ", #name); \
    name(); \
    g_tests_passed++; \
    printf("[PASS]\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL]\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        g_tests_failed++; g_tests_passed--; return; \
    } \
} while(0)

#define ASSERT_GT(a, b) do { \
    if (!((a) > (b))) { \
        printf("[FAIL]\n    Expected %s > %s, got %g vs %g\n    at %s:%d\n", \
               #a, #b, (double)(a), (double)(b), __FILE__, __LINE__); \
        g_tests_failed++; g_tests_passed--; return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("[FAIL]\n    Expected %s == %s\n    at %s:%d\n", \
               #a, #b, __FILE__, __LINE__); \
        g_tests_failed++; g_tests_passed--; return; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, tol) do { \
    if (fabs((double)(a) - (double)(b)) > (tol)) { \
        printf("[FAIL]\n    Expected %s ~= %s (tol=%g)\n" \
               "    got %g vs %g\n    at %s:%d\n", \
               #a, #b, (tol), (double)(a), (double)(b), \
               __FILE__, __LINE__); \
        g_tests_failed++; g_tests_passed--; return; \
    } \
} while(0)

/* ── Default test config ─────────────────────────────────────────────── */

static sim_config_t test_config(void)
{
    return (sim_config_t){
        .duration          = 7200.0,   /* 2 hours */
        .warmup            = 300.0,
        .num_lanes         = 3,
        .base_arrival_rate = 60.0,
        .base_service_rate = 30.0,
        .rush_hour_factor  = 1.0,      /* no rush for deterministic tests */
        .rush_start_hour   = 7,
        .rush_end_hour     = 10,
        .max_queue_capacity = 0,
        .seed              = 12345,
    };
}

/* ── Tests ───────────────────────────────────────────────────────────── */

/* Test 1: Simulator initialisation */
TEST(test_sim_init)
{
    simulator_t sim;
    sim_config_t cfg = test_config();
    ASSERT(sim_init(&sim, &cfg) == BQO_OK);
    ASSERT(sim.clock == 0.0);
    ASSERT(sim.total_arrivals == 0);
    ASSERT(sim.lanes[0].active);
    ASSERT(sim.lanes[1].active);
    ASSERT(sim.lanes[2].active);
    sim_destroy(&sim);
}

/* Test 2: Invalid configuration rejected */
TEST(test_sim_init_invalid)
{
    simulator_t sim;
    sim_config_t cfg = test_config();
    cfg.num_lanes = 0;
    ASSERT(sim_init(&sim, &cfg) == BQO_ERR_INVALID);

    cfg.num_lanes = BQO_MAX_LANES + 1;
    ASSERT(sim_init(&sim, &cfg) == BQO_ERR_INVALID);
}

/* Test 3: RNG produces values in [0, 1) */
TEST(test_sim_rng)
{
    simulator_t sim;
    sim_config_t cfg = test_config();
    sim_init(&sim, &cfg);

    for (int i = 0; i < 1000; i++) {
        double u = sim_rand_uniform(&sim);
        ASSERT(u >= 0.0 && u < 1.0);
    }

    sim_destroy(&sim);
}

/* Test 4: Exponential variates are positive */
TEST(test_sim_exponential)
{
    simulator_t sim;
    sim_config_t cfg = test_config();
    sim_init(&sim, &cfg);

    double sum = 0.0;
    int n = 5000;
    double mean = 10.0;
    for (int i = 0; i < n; i++) {
        double v = sim_rand_exponential(&sim, mean);
        ASSERT(v > 0.0);
        sum += v;
    }
    /* Check that the sample mean is roughly correct */
    double sample_mean = sum / (double)n;
    ASSERT(fabs(sample_mean - mean) < 1.0);

    sim_destroy(&sim);
}

/* Test 5: Full simulation produces reasonable results */
TEST(test_sim_run_basic)
{
    simulator_t sim;
    sim_config_t cfg = test_config();
    sim_init(&sim, &cfg);

    sim_result_t result;
    ASSERT(sim_run(&sim, &result) == BQO_OK);

    ASSERT_GT(result.total_arrivals, 0);
    ASSERT_GT(result.total_served, 0);
    ASSERT(result.avg_wait_time >= 0.0);
    ASSERT(result.utilization >= 0.0 && result.utilization <= 1.5);
    ASSERT_GT(result.throughput, 0.0);

    sim_destroy(&sim);
}

/* Test 6: Shortest queue lane selection */
TEST(test_sim_shortest_queue)
{
    simulator_t sim;
    sim_config_t cfg = test_config();
    sim_init(&sim, &cfg);

    sim.lanes[0].queue_length = 5;
    sim.lanes[1].queue_length = 2;
    sim.lanes[2].queue_length = 8;

    ASSERT_EQ(sim_shortest_queue_lane(&sim), 1);

    /* Deactivate lane 1 */
    sim.lanes[1].active = false;
    ASSERT_EQ(sim_shortest_queue_lane(&sim), 0);

    sim_destroy(&sim);
}

/* Test 7: More lanes should reduce wait times */
TEST(test_sim_more_lanes_less_wait)
{
    sim_config_t cfg = test_config();
    cfg.duration = 3600.0;
    cfg.base_arrival_rate = 80.0;
    cfg.base_service_rate = 30.0;

    sim_result_t result3, result5;

    cfg.num_lanes = 3;
    cfg.seed = 42;
    simulator_t sim3;
    sim_init(&sim3, &cfg);
    sim_run(&sim3, &result3);
    sim_destroy(&sim3);

    cfg.num_lanes = 5;
    cfg.seed = 42;
    simulator_t sim5;
    sim_init(&sim5, &cfg);
    sim_run(&sim5, &result5);
    sim_destroy(&sim5);

    /* More lanes should give lower or equal average wait */
    ASSERT(result5.avg_wait_time <= result3.avg_wait_time + 1.0);
}

/* ── Suite runner ────────────────────────────────────────────────────── */

void run_simulator_tests(void)
{
    printf("[Simulator Tests]\n");
    RUN_TEST(test_sim_init);
    RUN_TEST(test_sim_init_invalid);
    RUN_TEST(test_sim_rng);
    RUN_TEST(test_sim_exponential);
    RUN_TEST(test_sim_run_basic);
    RUN_TEST(test_sim_shortest_queue);
    RUN_TEST(test_sim_more_lanes_less_wait);
    printf("\n");
}
