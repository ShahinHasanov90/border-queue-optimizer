/*
 * Border Queue Optimizer
 * Tests for lane allocation optimizer
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <math.h>
#include "../src/optimizer.h"
#include "../src/queue_model.h"

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

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("[FAIL]\n    Expected %s == %s, got %d vs %d\n    at %s:%d\n", \
               #a, #b, (int)(a), (int)(b), __FILE__, __LINE__); \
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

#define ASSERT_NEAR(a, b, tol) do { \
    if (fabs((double)(a) - (double)(b)) > (tol)) { \
        printf("[FAIL]\n    Expected %s ~= %s (tol=%g)\n" \
               "    got %g vs %g\n    at %s:%d\n", \
               #a, #b, (tol), (double)(a), (double)(b), \
               __FILE__, __LINE__); \
        g_tests_failed++; g_tests_passed--; return; \
    } \
} while(0)

/* ── Tests ───────────────────────────────────────────────────────────── */

/* Test 1: Minimum stable lanes calculation */
TEST(test_opt_min_stable)
{
    /* lambda=120, mu=40 => need c > 3, so c_min = 4 */
    ASSERT_EQ(opt_min_stable_lanes(120.0, 40.0), 4);
    /* lambda=100, mu=50 => need c > 2, so c_min = 3 */
    ASSERT_EQ(opt_min_stable_lanes(100.0, 50.0), 3);
    /* lambda=39, mu=40 => c > 0.975, so c_min = 1 */
    ASSERT_EQ(opt_min_stable_lanes(39.0, 40.0), 1);
}

/* Test 2: Optimizer finds feasible solution */
TEST(test_opt_finds_solution)
{
    opt_constraints_t constraints = {
        .min_lanes = 1,
        .max_lanes = 16,
        .max_avg_wait = 300.0,
        .target_utilization = 0.9,
        .staff_cost_per_lane = 50.0,
        .wait_cost_per_sec = 0.10,
    };

    opt_result_t result;
    ASSERT(opt_find_optimal_lanes(120.0, 40.0, &constraints, &result) == BQO_OK);
    ASSERT(result.optimal_lanes >= 4);
    ASSERT(result.expected_avg_wait <= 300.0);
    ASSERT(result.expected_utilization < 0.9 + 0.01);
    ASSERT_GT(result.total_cost, 0.0);
}

/* Test 3: Infeasible constraints return error */
TEST(test_opt_infeasible)
{
    opt_constraints_t constraints = {
        .min_lanes = 1,
        .max_lanes = 2,      /* too few for lambda=120, mu=40 */
        .max_avg_wait = 60.0,
        .target_utilization = 0.5,
        .staff_cost_per_lane = 50.0,
        .wait_cost_per_sec = 0.10,
    };

    opt_result_t result;
    ASSERT(opt_find_optimal_lanes(120.0, 40.0, &constraints, &result) != BQO_OK);
}

/* Test 4: Higher arrival rate needs more lanes */
TEST(test_opt_more_traffic_more_lanes)
{
    opt_constraints_t constraints = {
        .min_lanes = 1,
        .max_lanes = 20,
        .max_avg_wait = 120.0,
        .target_utilization = 0.85,
        .staff_cost_per_lane = 50.0,
        .wait_cost_per_sec = 0.10,
    };

    opt_result_t r1, r2;
    ASSERT(opt_find_optimal_lanes(100.0, 40.0, &constraints, &r1) == BQO_OK);
    ASSERT(opt_find_optimal_lanes(200.0, 40.0, &constraints, &r2) == BQO_OK);
    ASSERT(r2.optimal_lanes >= r1.optimal_lanes);
}

/* Test 5: Total cost calculation */
TEST(test_opt_total_cost)
{
    double cost = opt_total_cost(120.0, 40.0, 4, 50.0, 0.10);
    ASSERT_GT(cost, 0.0);

    /* Staffing component: 4 * 50 = 200 */
    /* Waiting component: avg_wait * 120 * 0.10 */
    double staff_only = 50.0 * 4;
    ASSERT_GT(cost, staff_only);  /* total > staffing alone */

    /* Unstable system => infinite cost */
    double inf_cost = opt_total_cost(120.0, 40.0, 3, 50.0, 0.10);
    ASSERT(isinf(inf_cost));
}

/* Test 6: Optimizer respects min_lanes constraint */
TEST(test_opt_respects_min_lanes)
{
    opt_constraints_t constraints = {
        .min_lanes = 6,       /* Force at least 6 */
        .max_lanes = 16,
        .max_avg_wait = 600.0,
        .target_utilization = 0.95,
        .staff_cost_per_lane = 50.0,
        .wait_cost_per_sec = 0.10,
    };

    opt_result_t result;
    ASSERT(opt_find_optimal_lanes(120.0, 40.0, &constraints, &result) == BQO_OK);
    ASSERT(result.optimal_lanes >= 6);
}

/* ── Suite runner ────────────────────────────────────────────────────── */

void run_optimizer_tests(void)
{
    printf("[Optimizer Tests]\n");
    RUN_TEST(test_opt_min_stable);
    RUN_TEST(test_opt_finds_solution);
    RUN_TEST(test_opt_infeasible);
    RUN_TEST(test_opt_more_traffic_more_lanes);
    RUN_TEST(test_opt_total_cost);
    RUN_TEST(test_opt_respects_min_lanes);
    printf("\n");
}
