#ifndef LIB_RINGBUF_H
#define LIB_RINGBUF_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Fixed-storage, single-producer/single-consumer byte ring buffer.
 *
 * Initialise a ring once before either context uses it. The storage capacity
 * must be a non-zero power of two. The producer alone calls try_push() and
 * owns dropped_count; the consumer alone calls try_pop(). A diagnostic reader
 * may obtain a stale dropped-count snapshot. This implementation is for one
 * Cortex-M core and its interrupts; DMA and multi-core producers/consumers
 * require a different synchronization policy.
 */
typedef struct {
    uint8_t *storage;
    uint32_t capacity;
    uint32_t mask;
    volatile uint32_t write_index;
    volatile uint32_t read_index;
    volatile uint32_t dropped_count;
} lib_ringbuf_t;

bool lib_ringbuf_init(lib_ringbuf_t *ring, uint8_t *storage, uint32_t capacity);
bool lib_ringbuf_try_push(lib_ringbuf_t *ring, uint8_t byte);
bool lib_ringbuf_try_pop(lib_ringbuf_t *ring, uint8_t *byte);
uint32_t lib_ringbuf_capacity(const lib_ringbuf_t *ring);
uint32_t lib_ringbuf_dropped_count(const lib_ringbuf_t *ring);

#endif
