/*
 * Border Queue Optimizer
 * Min-heap priority queue implementation
 *
 * Binary min-heap ordered by event timestamp.  Supports O(log n) push
 * and pop, O(1) peek.  Automatically doubles capacity when full.
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include "heap.h"
#include <stdlib.h>
#include <string.h>

/* ── Internal helpers ────────────────────────────────────────────────── */

static inline int parent(int i) { return (i - 1) / 2; }
static inline int left(int i)   { return 2 * i + 1; }
static inline int right(int i)  { return 2 * i + 2; }

static inline void swap(event_t *a, event_t *b)
{
    event_t tmp = *a;
    *a = *b;
    *b = tmp;
}

static void sift_up(min_heap_t *heap, int idx)
{
    while (idx > 0 && heap->data[idx].time < heap->data[parent(idx)].time) {
        swap(&heap->data[idx], &heap->data[parent(idx)]);
        idx = parent(idx);
    }
}

static void sift_down(min_heap_t *heap, int idx)
{
    int n = heap->size;
    while (true) {
        int smallest = idx;
        int l = left(idx);
        int r = right(idx);

        if (l < n && heap->data[l].time < heap->data[smallest].time)
            smallest = l;
        if (r < n && heap->data[r].time < heap->data[smallest].time)
            smallest = r;

        if (smallest == idx)
            break;

        swap(&heap->data[idx], &heap->data[smallest]);
        idx = smallest;
    }
}

static bqo_error_t heap_grow(min_heap_t *heap)
{
    int new_cap = heap->capacity * 2;
    event_t *new_data = realloc(heap->data, (size_t)new_cap * sizeof(event_t));
    if (!new_data)
        return BQO_ERR_NOMEM;
    heap->data     = new_data;
    heap->capacity = new_cap;
    return BQO_OK;
}

/* ── Public API ──────────────────────────────────────────────────────── */

bqo_error_t heap_create(min_heap_t *heap, int capacity)
{
    if (!heap || capacity <= 0)
        return BQO_ERR_INVALID;

    heap->data = malloc((size_t)capacity * sizeof(event_t));
    if (!heap->data)
        return BQO_ERR_NOMEM;

    heap->size     = 0;
    heap->capacity = capacity;
    return BQO_OK;
}

void heap_destroy(min_heap_t *heap)
{
    if (heap) {
        free(heap->data);
        heap->data     = NULL;
        heap->size     = 0;
        heap->capacity = 0;
    }
}

bqo_error_t heap_push(min_heap_t *heap, event_t event)
{
    if (!heap)
        return BQO_ERR_INVALID;

    if (heap->size >= heap->capacity) {
        bqo_error_t err = heap_grow(heap);
        if (err != BQO_OK)
            return err;
    }

    heap->data[heap->size] = event;
    sift_up(heap, heap->size);
    heap->size++;
    return BQO_OK;
}

bqo_error_t heap_pop(min_heap_t *heap, event_t *out)
{
    if (!heap || !out)
        return BQO_ERR_INVALID;
    if (heap->size == 0)
        return BQO_ERR_EMPTY;

    *out = heap->data[0];
    heap->size--;
    if (heap->size > 0) {
        heap->data[0] = heap->data[heap->size];
        sift_down(heap, 0);
    }
    return BQO_OK;
}

bqo_error_t heap_peek(const min_heap_t *heap, event_t *out)
{
    if (!heap || !out)
        return BQO_ERR_INVALID;
    if (heap->size == 0)
        return BQO_ERR_EMPTY;

    *out = heap->data[0];
    return BQO_OK;
}

bool heap_is_empty(const min_heap_t *heap)
{
    return !heap || heap->size == 0;
}

int heap_size(const min_heap_t *heap)
{
    return heap ? heap->size : 0;
}
