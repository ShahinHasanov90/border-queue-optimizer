/*
 * Border Queue Optimizer
 * Tests for queue theory models (M/M/1, M/M/c, Erlang-C, Little's Law)
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <math.h>
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

#define ASSERT_NEAR(a, b, tol) do { \
    if (fabs((double)(a) - (double)(b)) > (tol)) { \
        printf("[FAIL]\n    Expected %s ~= %s (tol=%g)\n" \
               "    got %g vs %g\n    at %s:%d\n", \
               #a, #b, (tol), (double)(a), (double)(b), \
               __FILE__, __LINE__); \
        g_tests_failed++; g_tests_passed--; return; \
    } \
} while(0)

#define ASSERT_GT(a, b) do { \
    if (!((a) > (b))) { \
        printf("[FAIL]\n    Expected %s > %s\n    at %s:%d\n", \
               #a, #b, __FILE__, __LINE__); \
        g_tests_failed++; g_tests_passed--; return; \
    } \
} while(0)

/* ── M/M/1 Tests ─────────────────────────────────────────────────────── */

/* Test 1: M/M/1 utilization = lambda / mu */
TEST(test_mm1_utilization)
{
    ASSERT_NEAR(mm1_utilization(30.0, 40.0), 0.75, 1e-6);
    ASSERT_NEAR(mm1_utilization(10.0, 20.0), 0.50, 1e-6);
}

/* Test 2: M/M/1 average customers L = rho / (1 - rho) */
TEST(test_mm1_avg_customers)
{
    /* lambda=30, mu=40 => rho=0.75 => L = 0.75/0.25 = 3.0 */
    ASSERT_NEAR(mm1_avg_customers(30.0, 40.0), 3.0, 1e-6);
}

/* Test 3: M/M/1 average wait time Wq = rho / (mu - lambda) */
TEST(test_mm1_avg_wait_time)
{
    /* lambda=30, mu=40 => Wq = 0.75 / 10 = 0.075 hours */
    ASSERT_NEAR(mm1_avg_wait_time(30.0, 40.0), 0.075, 1e-6);
}

/* Test 4: M/M/1 unstable system (rho >= 1) returns INFINITY */
TEST(test_mm1_unstable)
{
    ASSERT(isinf(mm1_avg_customers(50.0, 40.0)));
    ASSERT(isinf(mm1_avg_wait_time(40.0, 40.0)));
}

/* ── M/M/c Tests ─────────────────────────────────────────────────────── */

/* Test 5: M/M/c utilization = lambda / (c * mu) */
TEST(test_mmc_utilization)
{
    /* lambda=120, mu=40, c=4 => rho = 120/(4*40) = 0.75 */
    ASSERT_NEAR(mmc_utilization(120.0, 40.0, 4), 0.75, 1e-6);
}

/* Test 6: Erlang-C for known values.
 * lambda=120, mu=40, c=4: a = 3, rho = 0.75
 * Verified against standard Erlang-C tables.                           */
TEST(test_mmc_erlang_c)
{
    double ec = mmc_erlang_c(120.0, 40.0, 4);
    /* Erlang-C(c=4, a=3) ~= 0.5094 (from standard tables) */
    ASSERT_NEAR(ec, 0.5094, 0.005);
    ASSERT(ec > 0.0 && ec <= 1.0);
}

/* Test 7: M/M/c average wait time decreases with more servers */
TEST(test_mmc_wait_decreases_with_servers)
{
    double wq4 = mmc_avg_wait_time(120.0, 40.0, 4);
    double wq5 = mmc_avg_wait_time(120.0, 40.0, 5);
    double wq6 = mmc_avg_wait_time(120.0, 40.0, 6);
    ASSERT_GT(wq4, wq5);
    ASSERT_GT(wq5, wq6);
}

/* Test 8: M/M/c overloaded returns infinity */
TEST(test_mmc_overloaded)
{
    /* lambda=120, mu=40, c=3 => rho = 1.0 => unstable */
    ASSERT(isinf(mmc_avg_queue_length(120.0, 40.0, 3)));
}

/* ── Little's Law Tests ──────────────────────────────────────────────── */

/* Test 9: L = lambda * W */
TEST(test_littles_law)
{
    ASSERT_NEAR(little_L(10.0, 0.5), 5.0, 1e-6);
    ASSERT_NEAR(little_lambda(5.0, 0.5), 10.0, 1e-6);
    ASSERT_NEAR(little_W(5.0, 10.0), 0.5, 1e-6);
}

/* ── Tail Probability ────────────────────────────────────────────────── */

/* Test 10: P(Wait > 0) should equal Erlang-C */
TEST(test_mmc_prob_wait_at_zero)
{
    double ec = mmc_erlang_c(120.0, 40.0, 4);
    double pw = mmc_prob_wait_exceeds(120.0, 40.0, 4, 0.0);
    ASSERT_NEAR(pw, ec, 1e-6);
}

/* Test 11: P(Wait > t) should decrease with t */
TEST(test_mmc_prob_wait_decreasing)
{
    double p1 = mmc_prob_wait_exceeds(120.0, 40.0, 4, 0.01);
    double p2 = mmc_prob_wait_exceeds(120.0, 40.0, 4, 0.05);
    double p3 = mmc_prob_wait_exceeds(120.0, 40.0, 4, 0.10);
    ASSERT_GT(p1, p2);
    ASSERT_GT(p2, p3);
}

/* ── Suite runner ────────────────────────────────────────────────────── */

void run_queue_model_tests(void)
{
    printf("[Queue Model Tests]\n");
    RUN_TEST(test_mm1_utilization);
    RUN_TEST(test_mm1_avg_customers);
    RUN_TEST(test_mm1_avg_wait_time);
    RUN_TEST(test_mm1_unstable);
    RUN_TEST(test_mmc_utilization);
    RUN_TEST(test_mmc_erlang_c);
    RUN_TEST(test_mmc_wait_decreases_with_servers);
    RUN_TEST(test_mmc_overloaded);
    RUN_TEST(test_littles_law);
    RUN_TEST(test_mmc_prob_wait_at_zero);
    RUN_TEST(test_mmc_prob_wait_decreasing);
    printf("\n");
}
