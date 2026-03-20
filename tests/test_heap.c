/*
 * Border Queue Optimizer
 * Tests for min-heap priority queue
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <math.h>
#include "../src/heap.h"

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

#define ASSERT_NEAR(a, b, tol) do { \
    if (fabs((double)(a) - (double)(b)) > (tol)) { \
        printf("[FAIL]\n    Expected %s ~= %s (tol=%g)\n" \
               "    got %g vs %g\n    at %s:%d\n", \
               #a, #b, (tol), (double)(a), (double)(b), \
               __FILE__, __LINE__); \
        g_tests_failed++; g_tests_passed--; return; \
    } \
} while(0)

/* Helper to make an event with a given time */
static event_t make_event(double time, event_type_t type)
{
    event_t ev = { .time = time, .type = type, .lane_id = -1,
                   .vehicle_id = -1, .vtype = VEHICLE_CAR };
    return ev;
}

/* ── Tests ───────────────────────────────────────────────────────────── */

/* Test 1: Create and destroy */
TEST(test_heap_create_destroy)
{
    min_heap_t heap;
    ASSERT(heap_create(&heap, 16) == BQO_OK);
    ASSERT(heap_is_empty(&heap));
    ASSERT_EQ(heap_size(&heap), 0);
    heap_destroy(&heap);
}

/* Test 2: Push and peek */
TEST(test_heap_push_peek)
{
    min_heap_t heap;
    heap_create(&heap, 8);

    heap_push(&heap, make_event(5.0, EVENT_ARRIVAL));
    heap_push(&heap, make_event(2.0, EVENT_DEPARTURE));
    heap_push(&heap, make_event(8.0, EVENT_ARRIVAL));

    event_t top;
    ASSERT(heap_peek(&heap, &top) == BQO_OK);
    ASSERT_NEAR(top.time, 2.0, 1e-9);
    ASSERT_EQ(heap_size(&heap), 3);

    heap_destroy(&heap);
}

/* Test 3: Pop returns events in ascending time order */
TEST(test_heap_pop_order)
{
    min_heap_t heap;
    heap_create(&heap, 8);

    double times[] = {7.0, 3.0, 9.0, 1.0, 5.0, 2.0, 8.0, 4.0, 6.0};
    int n = sizeof(times) / sizeof(times[0]);

    for (int i = 0; i < n; i++)
        heap_push(&heap, make_event(times[i], EVENT_ARRIVAL));

    double prev = -1.0;
    for (int i = 0; i < n; i++) {
        event_t ev;
        ASSERT(heap_pop(&heap, &ev) == BQO_OK);
        ASSERT(ev.time >= prev);
        prev = ev.time;
    }

    ASSERT(heap_is_empty(&heap));
    heap_destroy(&heap);
}

/* Test 4: Pop from empty heap returns BQO_ERR_EMPTY */
TEST(test_heap_pop_empty)
{
    min_heap_t heap;
    heap_create(&heap, 4);

    event_t ev;
    ASSERT(heap_pop(&heap, &ev) == BQO_ERR_EMPTY);

    heap_destroy(&heap);
}

/* Test 5: Automatic capacity growth */
TEST(test_heap_auto_grow)
{
    min_heap_t heap;
    heap_create(&heap, 2);  /* start very small */

    for (int i = 0; i < 100; i++)
        ASSERT(heap_push(&heap, make_event((double)i, EVENT_ARRIVAL)) == BQO_OK);

    ASSERT_EQ(heap_size(&heap), 100);

    /* Verify ordering still correct */
    for (int i = 0; i < 100; i++) {
        event_t ev;
        ASSERT(heap_pop(&heap, &ev) == BQO_OK);
        ASSERT_NEAR(ev.time, (double)i, 1e-9);
    }

    heap_destroy(&heap);
}

/* Test 6: Duplicate timestamps handled correctly */
TEST(test_heap_duplicate_times)
{
    min_heap_t heap;
    heap_create(&heap, 8);

    heap_push(&heap, make_event(5.0, EVENT_ARRIVAL));
    heap_push(&heap, make_event(5.0, EVENT_DEPARTURE));
    heap_push(&heap, make_event(5.0, EVENT_ARRIVAL));
    heap_push(&heap, make_event(3.0, EVENT_ARRIVAL));

    event_t ev;
    heap_pop(&heap, &ev);
    ASSERT_NEAR(ev.time, 3.0, 1e-9);

    /* Next three should all be 5.0 */
    for (int i = 0; i < 3; i++) {
        heap_pop(&heap, &ev);
        ASSERT_NEAR(ev.time, 5.0, 1e-9);
    }

    ASSERT(heap_is_empty(&heap));
    heap_destroy(&heap);
}

/* Test 7: Invalid create parameters */
TEST(test_heap_invalid_create)
{
    min_heap_t heap;
    ASSERT(heap_create(&heap, 0) == BQO_ERR_INVALID);
    ASSERT(heap_create(&heap, -1) == BQO_ERR_INVALID);
    ASSERT(heap_create(NULL, 10) == BQO_ERR_INVALID);
}

/* ── Suite runner ────────────────────────────────────────────────────── */

void run_heap_tests(void)
{
    printf("[Heap Tests]\n");
    RUN_TEST(test_heap_create_destroy);
    RUN_TEST(test_heap_push_peek);
    RUN_TEST(test_heap_pop_order);
    RUN_TEST(test_heap_pop_empty);
    RUN_TEST(test_heap_auto_grow);
    RUN_TEST(test_heap_duplicate_times);
    RUN_TEST(test_heap_invalid_create);
    printf("\n");
}
