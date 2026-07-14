#include <assert.h>
#include <stdint.h>

#include "lib_crash.h"
#include "lib_debug.h"
#include "lib_ringbuf.h"

static uint8_t g_debug_capture[4];
static uint32_t g_debug_capture_length;

static uint32_t test_debug_writer(const uint8_t *data, uint32_t length) {
    uint32_t index;

    assert(length <= (uint32_t)sizeof(g_debug_capture));
    for (index = 0U; index < length; ++index) {
        g_debug_capture[index] = data[index];
    }

    g_debug_capture_length = length;
    return length;
}

static void test_rejects_invalid_storage_and_capacity(void) {
    lib_ringbuf_t ring = {0};
    uint8_t storage[4] = {0};

    assert(!lib_ringbuf_init(0, storage, UINT32_C(4)));
    assert(!lib_ringbuf_init(&ring, 0, UINT32_C(4)));
    assert(!lib_ringbuf_init(&ring, storage, 0U));
    assert(!lib_ringbuf_init(&ring, storage, UINT32_C(3)));
    assert(lib_ringbuf_init(&ring, storage, UINT32_C(1)));
    assert(lib_ringbuf_capacity(&ring) == UINT32_C(1));
}

static void test_empty_and_null_operations(void) {
    lib_ringbuf_t ring = {0};
    uint8_t storage[4] = {0};
    uint8_t byte = 0U;

    assert(lib_ringbuf_init(&ring, storage, UINT32_C(4)));
    assert(!lib_ringbuf_try_pop(&ring, &byte));
    assert(!lib_ringbuf_try_pop(&ring, 0));
    assert(!lib_ringbuf_try_push(0, UINT8_C(0x42)));
    assert(lib_ringbuf_capacity(0) == 0U);
    assert(lib_ringbuf_dropped_count(0) == 0U);
}

static void test_full_queue_preserves_fifo_data_and_counts_drop(void) {
    lib_ringbuf_t ring = {0};
    uint8_t storage[4] = {0};
    uint8_t byte = 0U;

    assert(lib_ringbuf_init(&ring, storage, UINT32_C(4)));
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x10)));
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x11)));
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x12)));
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x13)));
    assert(!lib_ringbuf_try_push(&ring, UINT8_C(0x14)));
    assert(lib_ringbuf_dropped_count(&ring) == UINT32_C(1));

    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x10)));
    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x11)));
    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x12)));
    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x13)));
    assert(!lib_ringbuf_try_pop(&ring, &byte));
}

static void test_wraparound_and_index_overflow(void) {
    lib_ringbuf_t ring = {0};
    uint8_t storage[4] = {0};
    uint8_t byte = 0U;

    assert(lib_ringbuf_init(&ring, storage, UINT32_C(4)));
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x20)));
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x21)));
    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x20)));
    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x21)));
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x22)));
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x23)));
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x24)));
    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x22)));
    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x23)));
    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x24)));

    ring.write_index = UINT32_MAX - UINT32_C(1);
    ring.read_index = UINT32_MAX - UINT32_C(1);
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x30)));
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x31)));
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x32)));
    assert(lib_ringbuf_try_push(&ring, UINT8_C(0x33)));
    assert(!lib_ringbuf_try_push(&ring, UINT8_C(0x34)));
    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x30)));
    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x31)));
    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x32)));
    assert(lib_ringbuf_try_pop(&ring, &byte) && (byte == UINT8_C(0x33)));
}

static void test_debug_transport_is_register_free_callback(void) {
    static const uint8_t message[] = {UINT8_C(0x41), UINT8_C(0x42), UINT8_C(0x43)};

    lib_debug_set_writer(0);
    assert(lib_debug_write(message, (uint32_t)sizeof(message)) == 0U);
    assert(lib_debug_write(0, UINT32_C(1)) == 0U);

    g_debug_capture_length = 0U;
    lib_debug_set_writer(test_debug_writer);
    assert(lib_debug_write(message, (uint32_t)sizeof(message)) ==
           (uint32_t)sizeof(message));
    assert(g_debug_capture_length == (uint32_t)sizeof(message));
    assert(g_debug_capture[0] == UINT8_C(0x41));
    assert(g_debug_capture[1] == UINT8_C(0x42));
    assert(g_debug_capture[2] == UINT8_C(0x43));
    assert(lib_debug_write(0, 0U) == 0U);
    lib_debug_set_writer(0);
}

static void test_crash_record_rejects_garbage_and_preserves_fault(void) {
    lib_crash_record_t record = {0};

    assert(!lib_crash_is_valid(&record));
    assert(!lib_crash_has_fault(&record));
    lib_crash_note_boot(&record, UINT32_C(0x0D));
    assert(lib_crash_is_valid(&record));
    assert(!lib_crash_has_fault(&record));
    assert(record.sequence == 0U);
    assert(record.reset_cause == UINT32_C(0x0D));

    lib_crash_capture(&record, LIB_CRASH_REASON_HARDFAULT, UINT32_C(3),
                      UINT32_C(0x12345678), UINT32_C(0x21000003));
    assert(lib_crash_is_valid(&record));
    assert(lib_crash_has_fault(&record));
    assert(record.sequence == UINT32_C(1));
    assert(record.reset_cause == UINT32_C(0x0D));
    assert(lib_crash_reason(&record) == LIB_CRASH_REASON_HARDFAULT);
    assert(lib_crash_exception_number(&record) == UINT32_C(3));
    assert(record.stacked_pc == UINT32_C(0x12345678));
    assert(record.stacked_xpsr == UINT32_C(0x21000003));

    lib_crash_note_boot(&record, UINT32_C(0x0E));
    assert(lib_crash_is_valid(&record));
    assert(lib_crash_has_fault(&record));
    assert(record.sequence == UINT32_C(1));
    assert(record.reset_cause == UINT32_C(0x0E));

    record.stacked_pc ^= UINT32_C(1);
    assert(!lib_crash_is_valid(&record));
    assert(!lib_crash_has_fault(&record));
}

static void test_crash_record_restarts_sequence_after_invalid_record(void) {
    lib_crash_record_t record = {0};

    lib_crash_capture(&record, LIB_CRASH_REASON_NMI, UINT32_C(2), UINT32_C(0x100),
                      UINT32_C(0x01000002));
    assert(lib_crash_is_valid(&record));
    assert(record.sequence == UINT32_C(1));

    record.magic = 0U;
    lib_crash_capture(&record, LIB_CRASH_REASON_UNEXPECTED_EXCEPTION, UINT32_C(31),
                      UINT32_C(0x200), UINT32_C(0x01000003));
    assert(lib_crash_is_valid(&record));
    assert(record.sequence == UINT32_C(1));
    assert(record.reset_cause == 0U);
    assert(lib_crash_reason(&record) == LIB_CRASH_REASON_UNEXPECTED_EXCEPTION);
    assert(lib_crash_exception_number(&record) == UINT32_C(31));
}

int main(void) {
    _Static_assert(sizeof(uint32_t) == 4U, "uint32_t must be exactly 32 bits");
    _Static_assert(sizeof(uint8_t) == 1U, "uint8_t must be exactly 8 bits");

    test_rejects_invalid_storage_and_capacity();
    test_empty_and_null_operations();
    test_full_queue_preserves_fifo_data_and_counts_drop();
    test_wraparound_and_index_overflow();
    test_debug_transport_is_register_free_callback();
    test_crash_record_rejects_garbage_and_preserves_fault();
    test_crash_record_restarts_sequence_after_invalid_record();
    return 0;
}
