#include "lib_ringbuf.h"

#include <stdatomic.h>

static bool lib_ringbuf_is_power_of_two(uint32_t value) {
    return (value != 0U) && ((value & (value - UINT32_C(1))) == 0U);
}

bool lib_ringbuf_init(lib_ringbuf_t *ring, uint8_t *storage, uint32_t capacity) {
    if ((ring == 0) || (storage == 0) || !lib_ringbuf_is_power_of_two(capacity)) {
        return false;
    }

    ring->storage = storage;
    ring->capacity = capacity;
    ring->mask = capacity - UINT32_C(1);
    ring->write_index = 0U;
    ring->read_index = 0U;
    ring->dropped_count = 0U;
    return true;
}

bool lib_ringbuf_try_push(lib_ringbuf_t *ring, uint8_t byte) {
    uint32_t write_index;

    if (ring == 0) {
        return false;
    }

    write_index = ring->write_index;
    if ((write_index - ring->read_index) == ring->capacity) {
        ring->dropped_count++;
        return false;
    }

    ring->storage[write_index & ring->mask] = byte;
    atomic_signal_fence(memory_order_release);
    ring->write_index = write_index + UINT32_C(1);
    return true;
}

bool lib_ringbuf_try_pop(lib_ringbuf_t *ring, uint8_t *byte) {
    uint32_t read_index;

    if ((ring == 0) || (byte == 0)) {
        return false;
    }

    read_index = ring->read_index;
    if (read_index == ring->write_index) {
        return false;
    }

    atomic_signal_fence(memory_order_acquire);
    *byte = ring->storage[read_index & ring->mask];
    atomic_signal_fence(memory_order_release);
    ring->read_index = read_index + UINT32_C(1);
    return true;
}

uint32_t lib_ringbuf_capacity(const lib_ringbuf_t *ring) {
    return (ring == 0) ? 0U : ring->capacity;
}

uint32_t lib_ringbuf_dropped_count(const lib_ringbuf_t *ring) {
    return (ring == 0) ? 0U : ring->dropped_count;
}
