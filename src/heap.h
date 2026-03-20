/*
 * Border Queue Optimizer
 * Min-heap priority queue for event scheduling
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#ifndef BQO_HEAP_H
#define BQO_HEAP_H

#include "../include/common.h"

/* Min-heap backed by a dynamic array of events.
 * Events are ordered by timestamp (ascending).                         */
typedef struct {
    event_t *data;
    int      size;
    int      capacity;
} min_heap_t;

/* Create a heap with the given initial capacity.
 * Returns BQO_OK on success, BQO_ERR_NOMEM on allocation failure.     */
bqo_error_t heap_create(min_heap_t *heap, int capacity);

/* Destroy a heap, freeing its internal buffer.                         */
void heap_destroy(min_heap_t *heap);

/* Insert an event into the heap.
 * Returns BQO_OK or BQO_ERR_NOMEM if growth fails.                    */
bqo_error_t heap_push(min_heap_t *heap, event_t event);

/* Remove and return the minimum (earliest) event.
 * Returns BQO_OK or BQO_ERR_EMPTY if the heap is empty.               */
bqo_error_t heap_pop(min_heap_t *heap, event_t *out);

/* Peek at the minimum event without removing it.
 * Returns BQO_OK or BQO_ERR_EMPTY.                                    */
bqo_error_t heap_peek(const min_heap_t *heap, event_t *out);

/* Return true if the heap contains no events.                          */
bool heap_is_empty(const min_heap_t *heap);

/* Return the number of events in the heap.                             */
int heap_size(const min_heap_t *heap);

#endif /* BQO_HEAP_H */
