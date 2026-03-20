/*
 * Border Queue Optimizer
 * Test runner with assertion macros
 *
 * Lightweight test framework: runs all registered test suites,
 * reports pass/fail counts, returns non-zero exit code on failure.
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ── Test framework macros ───────────────────────────────────────────── */

int g_tests_run    = 0;
int g_tests_passed = 0;
int g_tests_failed = 0;

#define TEST(name) \
    static void name(void); \
    static void name(void)

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
        g_tests_failed++; \
        g_tests_passed--; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("[FAIL]\n    Expected %s == %s\n    at %s:%d\n", \
               #a, #b, __FILE__, __LINE__); \
        g_tests_failed++; \
        g_tests_passed--; \
        return; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, tol) do { \
    if (fabs((double)(a) - (double)(b)) > (tol)) { \
        printf("[FAIL]\n    Expected %s ~= %s (tol=%g)\n" \
               "    got %g vs %g\n    at %s:%d\n", \
               #a, #b, (tol), (double)(a), (double)(b), \
               __FILE__, __LINE__); \
        g_tests_failed++; \
        g_tests_passed--; \
        return; \
    } \
} while(0)

#define ASSERT_GT(a, b) do { \
    if (!((a) > (b))) { \
        printf("[FAIL]\n    Expected %s > %s\n    at %s:%d\n", \
               #a, #b, __FILE__, __LINE__); \
        g_tests_failed++; \
        g_tests_passed--; \
        return; \
    } \
} while(0)

#define ASSERT_LT(a, b) do { \
    if (!((a) < (b))) { \
        printf("[FAIL]\n    Expected %s < %s\n    at %s:%d\n", \
               #a, #b, __FILE__, __LINE__); \
        g_tests_failed++; \
        g_tests_passed--; \
        return; \
    } \
} while(0)

/* ── External test suites ────────────────────────────────────────────── */

extern void run_queue_model_tests(void);
extern void run_heap_tests(void);
extern void run_simulator_tests(void);
extern void run_optimizer_tests(void);

/* ── Main ────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("========================================\n");
    printf("  Border Queue Optimizer - Test Suite\n");
    printf("========================================\n\n");

    run_queue_model_tests();
    run_heap_tests();
    run_simulator_tests();
    run_optimizer_tests();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed, %d total\n",
           g_tests_passed, g_tests_failed, g_tests_run);
    printf("========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
