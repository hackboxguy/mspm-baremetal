#include "lib_regmap.h"

#include <stddef.h>

#define LIB_REGMAP_SNAPSHOT_ACTIVE_MASK UINT32_C(0x00000001)
#define LIB_REGMAP_SNAPSHOT_READER_ZERO_SHIFT UINT32_C(1)
#define LIB_REGMAP_SNAPSHOT_READER_ONE_SHIFT UINT32_C(16)
#define LIB_REGMAP_SNAPSHOT_READER_MASK UINT32_C(0x00007fff)

static bool lib_regmap_is_power_of_two(uint32_t value) {
    return (value != 0U) && ((value & (value - UINT32_C(1))) == 0U);
}

static uint32_t lib_regmap_snapshot_reader_shift(uint8_t buffer_index) {
    return buffer_index == 0U ? LIB_REGMAP_SNAPSHOT_READER_ZERO_SHIFT
                              : LIB_REGMAP_SNAPSHOT_READER_ONE_SHIFT;
}

static uint32_t lib_regmap_snapshot_reader_count(uint32_t state, uint8_t buffer_index) {
    return (state >> lib_regmap_snapshot_reader_shift(buffer_index)) &
           LIB_REGMAP_SNAPSHOT_READER_MASK;
}

static bool lib_regmap_snapshot_acquire(lib_regmap_snapshot_t *snapshot,
                                        uint8_t *buffer_index) {
    uint32_t state;
    uint32_t desired;
    uint8_t active_index;
    uint32_t reader_increment;

    if ((snapshot == NULL) || (buffer_index == NULL)) {
        return false;
    }

    state = (uint32_t)atomic_load_explicit(&snapshot->state, memory_order_acquire);
    for (;;) {
        active_index = (uint8_t)(state & LIB_REGMAP_SNAPSHOT_ACTIVE_MASK);
        if (lib_regmap_snapshot_reader_count(state, active_index) ==
            LIB_REGMAP_SNAPSHOT_READER_MASK) {
            return false;
        }

        reader_increment = UINT32_C(1)
                           << lib_regmap_snapshot_reader_shift(active_index);
        desired = state + reader_increment;
        if (atomic_compare_exchange_weak_explicit(&snapshot->state, &state, desired,
                                                  memory_order_acquire,
                                                  memory_order_acquire)) {
            *buffer_index = active_index;
            return true;
        }
    }
}

static void lib_regmap_snapshot_release(lib_regmap_snapshot_t *snapshot,
                                        uint8_t buffer_index) {
    const uint32_t reader_decrement = UINT32_C(1)
                                      << lib_regmap_snapshot_reader_shift(buffer_index);

    if (snapshot != NULL) {
        (void)atomic_fetch_sub_explicit(&snapshot->state, reader_decrement,
                                        memory_order_release);
    }
}

static bool lib_regmap_snapshot_is_valid(const lib_regmap_snapshot_t *snapshot) {
    const uint32_t state =
        snapshot == NULL
            ? UINT32_C(0xffffffff)
            : (uint32_t)atomic_load_explicit(&snapshot->state, memory_order_relaxed);

    return (snapshot != NULL) && (snapshot->buffers[0] != NULL) &&
           (snapshot->buffers[1] != NULL) && (snapshot->length != 0U) &&
           ((state & LIB_REGMAP_SNAPSHOT_ACTIVE_MASK) <= UINT32_C(1));
}

static const lib_regmap_page_t *lib_regmap_find_page(const lib_regmap_t *regmap,
                                                     uint16_t address) {
    uint16_t index;

    if (regmap == NULL) {
        return NULL;
    }

    for (index = 0U; index < regmap->page_count; ++index) {
        const lib_regmap_page_t *const page = &regmap->pages[index];
        const uint32_t page_end = (uint32_t)page->first_address + page->length;

        if (((uint32_t)address >= page->first_address) &&
            ((uint32_t)address < page_end)) {
            return page;
        }
    }

    return NULL;
}

bool lib_regmap_command_queue_init(lib_regmap_command_queue_t *queue,
                                   lib_regmap_command_t *storage, uint32_t capacity) {
    if ((queue == NULL) || (storage == NULL) || !lib_regmap_is_power_of_two(capacity)) {
        return false;
    }

    queue->storage = storage;
    queue->capacity = capacity;
    queue->write_index = 0U;
    queue->read_index = 0U;
    queue->dropped_count = 0U;
    return true;
}

bool lib_regmap_command_queue_enqueue(void *context, uint16_t address, uint8_t value) {
    lib_regmap_command_queue_t *const queue = context;
    const uint32_t write_index = queue == NULL ? 0U : queue->write_index;
    const uint32_t read_index = queue == NULL ? 0U : queue->read_index;

    if ((queue == NULL) || (queue->storage == NULL) ||
        !lib_regmap_is_power_of_two(queue->capacity)) {
        return false;
    }

    if ((write_index - read_index) >= queue->capacity) {
        ++queue->dropped_count;
        return false;
    }

    queue->storage[write_index & (queue->capacity - UINT32_C(1))].address = address;
    queue->storage[write_index & (queue->capacity - UINT32_C(1))].value = value;
    atomic_signal_fence(memory_order_release);
    queue->write_index = write_index + UINT32_C(1);
    return true;
}

bool lib_regmap_command_queue_try_pop(lib_regmap_command_queue_t *queue,
                                      lib_regmap_command_t *command) {
    const uint32_t read_index = queue == NULL ? 0U : queue->read_index;
    const uint32_t write_index = queue == NULL ? 0U : queue->write_index;

    if ((queue == NULL) || (command == NULL) || (queue->storage == NULL) ||
        !lib_regmap_is_power_of_two(queue->capacity) || (read_index == write_index)) {
        return false;
    }

    atomic_signal_fence(memory_order_acquire);
    *command = queue->storage[read_index & (queue->capacity - UINT32_C(1))];
    queue->read_index = read_index + UINT32_C(1);
    return true;
}

uint32_t
lib_regmap_command_queue_dropped_count(const lib_regmap_command_queue_t *queue) {
    return queue == NULL ? 0U : queue->dropped_count;
}

bool lib_regmap_snapshot_init(lib_regmap_snapshot_t *snapshot, uint8_t *buffer_zero,
                              uint8_t *buffer_one, uint16_t length,
                              const uint8_t *initial_data) {
    uint16_t index;

    if ((snapshot == NULL) || (buffer_zero == NULL) || (buffer_one == NULL) ||
        (length == 0U)) {
        return false;
    }

    snapshot->buffers[0] = buffer_zero;
    snapshot->buffers[1] = buffer_one;
    snapshot->length = length;
    for (index = 0U; index < length; ++index) {
        const uint8_t value = initial_data == NULL ? 0U : initial_data[index];

        buffer_zero[index] = value;
        buffer_one[index] = value;
    }
    atomic_init(&snapshot->state, 0U);
    return true;
}

bool lib_regmap_snapshot_publish(lib_regmap_snapshot_t *snapshot, const uint8_t *data,
                                 uint16_t length) {
    uint32_t state;
    uint32_t desired;
    uint8_t inactive_index;
    uint16_t index;

    if (!lib_regmap_snapshot_is_valid(snapshot) || (data == NULL) ||
        (length != snapshot->length)) {
        return false;
    }

    state = (uint32_t)atomic_load_explicit(&snapshot->state, memory_order_acquire);
    inactive_index = (uint8_t)((state & LIB_REGMAP_SNAPSHOT_ACTIVE_MASK) ^ UINT32_C(1));
    if (lib_regmap_snapshot_reader_count(state, inactive_index) != 0U) {
        return false;
    }

    for (index = 0U; index < length; ++index) {
        snapshot->buffers[inactive_index][index] = data[index];
    }
    atomic_signal_fence(memory_order_release);

    for (;;) {
        if (((state & LIB_REGMAP_SNAPSHOT_ACTIVE_MASK) ^ UINT32_C(1)) !=
            inactive_index) {
            return false;
        }
        desired = state ^ LIB_REGMAP_SNAPSHOT_ACTIVE_MASK;
        if (atomic_compare_exchange_weak_explicit(&snapshot->state, &state, desired,
                                                  memory_order_release,
                                                  memory_order_acquire)) {
            return true;
        }
    }
}

bool lib_regmap_init(lib_regmap_t *regmap, const lib_regmap_page_t *pages,
                     uint16_t page_count) {
    uint16_t index;
    uint32_t previous_end = 0U;

    if ((regmap == NULL) || (pages == NULL) || (page_count == 0U)) {
        return false;
    }

    for (index = 0U; index < page_count; ++index) {
        const lib_regmap_page_t *const page = &pages[index];
        const uint32_t page_end = (uint32_t)page->first_address + page->length;

        if ((page->length == 0U) || (page_end > UINT32_C(0x10000)) ||
            ((uint32_t)page->first_address < previous_end) ||
            !lib_regmap_snapshot_is_valid(page->snapshot) ||
            (page->snapshot->length != page->length) ||
            ((page->access != LIB_REGMAP_ACCESS_READ_ONLY) &&
             (page->access != LIB_REGMAP_ACCESS_READ_WRITE)) ||
            ((page->access == LIB_REGMAP_ACCESS_READ_WRITE) &&
             (page->queue_write == NULL))) {
            return false;
        }
        previous_end = page_end;
    }

    regmap->pages = pages;
    regmap->page_count = page_count;
    regmap->current_address = 0U;
    regmap->latched_page = NULL;
    regmap->latched_buffer = 0U;
    return true;
}

void lib_regmap_set_address(lib_regmap_t *regmap, uint16_t address) {
    if (regmap == NULL) {
        return;
    }

    lib_regmap_end_read(regmap);
    regmap->current_address = address;
}

uint16_t lib_regmap_current_address(const lib_regmap_t *regmap) {
    return regmap == NULL ? 0U : regmap->current_address;
}

void lib_regmap_begin_read(lib_regmap_t *regmap) {
    lib_regmap_end_read(regmap);
}

void lib_regmap_end_read(lib_regmap_t *regmap) {
    if ((regmap != NULL) && (regmap->latched_page != NULL)) {
        lib_regmap_snapshot_release(regmap->latched_page->snapshot,
                                    regmap->latched_buffer);
        regmap->latched_page = NULL;
        regmap->latched_buffer = 0U;
    }
}

uint8_t lib_regmap_read_current(lib_regmap_t *regmap) {
    const lib_regmap_page_t *page;
    uint8_t value = LIB_REGMAP_UNMAPPED_VALUE;

    if (regmap == NULL) {
        return value;
    }

    page = lib_regmap_find_page(regmap, regmap->current_address);
    if (page != regmap->latched_page) {
        lib_regmap_end_read(regmap);
        if ((page != NULL) &&
            lib_regmap_snapshot_acquire(page->snapshot, &regmap->latched_buffer)) {
            regmap->latched_page = page;
        }
    }

    if (regmap->latched_page != NULL) {
        value =
            regmap->latched_page->snapshot
                ->buffers[regmap->latched_buffer][regmap->current_address -
                                                  regmap->latched_page->first_address];
    }
    ++regmap->current_address;
    return value;
}

lib_regmap_write_result_t lib_regmap_write_current(lib_regmap_t *regmap,
                                                   uint8_t value) {
    const lib_regmap_page_t *page;
    lib_regmap_write_result_t result = LIB_REGMAP_WRITE_IGNORED;

    if (regmap == NULL) {
        return result;
    }

    lib_regmap_end_read(regmap);
    page = lib_regmap_find_page(regmap, regmap->current_address);
    if ((page != NULL) && (page->access == LIB_REGMAP_ACCESS_READ_WRITE)) {
        result = page->queue_write(page->context, regmap->current_address, value)
                     ? LIB_REGMAP_WRITE_QUEUED
                     : LIB_REGMAP_WRITE_REJECTED;
    }
    ++regmap->current_address;
    return result;
}
