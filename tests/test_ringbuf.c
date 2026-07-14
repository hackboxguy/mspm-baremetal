#include <assert.h>
#include <stdint.h>

#include "lib_boot.h"
#include "lib_buildinfo.h"
#include "lib_crash.h"
#include "lib_debug.h"
#include "lib_regmap.h"
#include "lib_ringbuf.h"

static uint8_t g_debug_capture[128];
static uint32_t g_debug_capture_length;

static uint32_t test_debug_writer(const uint8_t *data, uint32_t length) {
    uint32_t index;

    assert((g_debug_capture_length + length) <= (uint32_t)sizeof(g_debug_capture));
    for (index = 0U; index < length; ++index) {
        g_debug_capture[g_debug_capture_length + index] = data[index];
    }

    g_debug_capture_length += length;
    return length;
}

static void test_debug_capture_matches(const char *expected) {
    uint32_t index = 0U;

    while (expected[index] != '\0') {
        assert(index < g_debug_capture_length);
        assert(g_debug_capture[index] == (uint8_t)expected[index]);
        ++index;
    }
    assert(index == g_debug_capture_length);
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

static void test_buildinfo_and_boot_banner(void) {
    const lib_buildinfo_t buildinfo = {
        .version_major = UINT8_C(0x01),
        .version_minor = UINT8_C(0x02),
        .year_high = UINT8_C(0x20),
        .year_low = UINT8_C(0x26),
        .month = UINT8_C(0x07),
        .day = UINT8_C(0x14),
        .hour = UINT8_C(0x09),
        .minute = UINT8_C(0x45),
    };
    const lib_buildinfo_t reproducible_build = {
        .version_major = UINT8_C(0x01),
        .version_minor = UINT8_C(0x02),
    };
    lib_buildinfo_t invalid_build = buildinfo;
    uint8_t device_info[LIB_BUILDINFO_DEVICE_INFO_SIZE] = {0};

    assert(lib_buildinfo_is_bcd(UINT8_C(0x99)));
    assert(!lib_buildinfo_is_bcd(UINT8_C(0x1a)));
    assert(lib_buildinfo_is_valid(&buildinfo));
    assert(lib_buildinfo_has_timestamp(&buildinfo));
    assert(lib_buildinfo_is_valid(&reproducible_build));
    assert(!lib_buildinfo_has_timestamp(&reproducible_build));
    assert(lib_buildinfo_write_device_info(&buildinfo, device_info,
                                           (uint32_t)sizeof(device_info)));
    assert(device_info[0] == UINT8_C(0x01));
    assert(device_info[1] == UINT8_C(0x02));
    assert(device_info[2] == UINT8_C(0x20));
    assert(device_info[7] == UINT8_C(0x45));

    invalid_build.month = UINT8_C(0x13);
    assert(!lib_buildinfo_is_valid(&invalid_build));
    assert(!lib_buildinfo_write_device_info(&invalid_build, device_info,
                                            (uint32_t)sizeof(device_info)));

    g_debug_capture_length = 0U;
    lib_debug_set_writer(test_debug_writer);
    lib_boot_write_banner("regmap", &buildinfo);
    test_debug_capture_matches(
        "mspm-baremetal: regmap v01.02 build=2026-07-14T09:45Z\r\n");

    g_debug_capture_length = 0U;
    lib_boot_write_banner(0, &reproducible_build);
    test_debug_capture_matches("mspm-baremetal: unnamed v01.02 build=unspecified\r\n");
    lib_debug_set_writer(0);
}

static void test_regmap_command_queue(void) {
    lib_regmap_command_queue_t queue = {0};
    lib_regmap_command_t storage[2] = {0};
    lib_regmap_command_t command = {0};

    assert(!lib_regmap_command_queue_init(0, storage, UINT32_C(2)));
    assert(!lib_regmap_command_queue_init(&queue, 0, UINT32_C(2)));
    assert(!lib_regmap_command_queue_init(&queue, storage, UINT32_C(3)));
    queue.storage = storage;
    queue.capacity = UINT32_C(3);
    assert(!lib_regmap_command_queue_enqueue(&queue, UINT16_C(0x0200), UINT8_C(0x11)));
    assert(!lib_regmap_command_queue_try_pop(&queue, &command));
    assert(lib_regmap_command_queue_init(&queue, storage, UINT32_C(2)));

    assert(lib_regmap_command_queue_enqueue(&queue, UINT16_C(0x0200), UINT8_C(0x11)));
    assert(lib_regmap_command_queue_enqueue(&queue, UINT16_C(0x0201), UINT8_C(0x12)));
    assert(!lib_regmap_command_queue_enqueue(&queue, UINT16_C(0x0202), UINT8_C(0x13)));
    assert(lib_regmap_command_queue_dropped_count(&queue) == UINT32_C(1));

    assert(lib_regmap_command_queue_try_pop(&queue, &command));
    assert(command.address == UINT16_C(0x0200));
    assert(command.value == UINT8_C(0x11));
    assert(lib_regmap_command_queue_try_pop(&queue, &command));
    assert(command.address == UINT16_C(0x0201));
    assert(command.value == UINT8_C(0x12));
    assert(!lib_regmap_command_queue_try_pop(&queue, &command));
}

static void test_regmap_rejects_invalid_page_layout(void) {
    uint8_t buffer_zero[2] = {0};
    uint8_t buffer_one[2] = {0};
    lib_regmap_snapshot_t snapshot = {0};
    lib_regmap_t regmap = {0};
    lib_regmap_page_t pages[2] = {
        {
            .first_address = UINT16_C(0x0010),
            .length = UINT16_C(2),
            .access = LIB_REGMAP_ACCESS_READ_ONLY,
            .snapshot = &snapshot,
        },
    };

    assert(!lib_regmap_snapshot_init(0, buffer_zero, buffer_one,
                                     (uint16_t)sizeof(buffer_zero), buffer_zero));
    assert(!lib_regmap_snapshot_init(&snapshot, 0, buffer_one,
                                     (uint16_t)sizeof(buffer_zero), buffer_zero));
    assert(lib_regmap_snapshot_init(&snapshot, buffer_zero, buffer_one,
                                    (uint16_t)sizeof(buffer_zero), buffer_zero));
    assert(!lib_regmap_snapshot_publish(&snapshot, buffer_zero, UINT16_C(1)));
    assert(!lib_regmap_init(0, pages, UINT16_C(1)));
    assert(!lib_regmap_init(&regmap, 0, UINT16_C(1)));
    assert(!lib_regmap_init(&regmap, pages, 0U));

    pages[0].length = 0U;
    assert(!lib_regmap_init(&regmap, pages, UINT16_C(1)));
    pages[0].length = UINT16_C(2);
    pages[0].access = LIB_REGMAP_ACCESS_READ_WRITE;
    assert(!lib_regmap_init(&regmap, pages, UINT16_C(1)));
    pages[0].access = LIB_REGMAP_ACCESS_READ_ONLY;
    pages[0].first_address = UINT16_C(0xffff);
    assert(!lib_regmap_init(&regmap, pages, UINT16_C(1)));
    pages[0].first_address = UINT16_C(0x0010);

    pages[1] = pages[0];
    pages[1].first_address = UINT16_C(0x0011);
    assert(!lib_regmap_init(&regmap, pages, UINT16_C(2)));
}

static void test_regmap_access_rules_and_snapshot_latching(void) {
    uint8_t read_zero[2] = {UINT8_C(0x10), UINT8_C(0x11)};
    uint8_t read_one[2] = {0};
    uint8_t control_zero[2] = {UINT8_C(0x20), UINT8_C(0x21)};
    uint8_t control_one[2] = {0};
    uint8_t wrap_zero[2] = {UINT8_C(0xfe), UINT8_C(0xff)};
    uint8_t wrap_one[2] = {0};
    const uint8_t updated_read[2] = {UINT8_C(0x30), UINT8_C(0x31)};
    const uint8_t later_read[2] = {UINT8_C(0x40), UINT8_C(0x41)};
    lib_regmap_snapshot_t read_snapshot = {0};
    lib_regmap_snapshot_t control_snapshot = {0};
    lib_regmap_snapshot_t wrap_snapshot = {0};
    lib_regmap_command_t command_storage[4] = {0};
    lib_regmap_command_queue_t command_queue = {0};
    const lib_regmap_page_t pages[] = {
        {
            .first_address = UINT16_C(0x0000),
            .length = UINT16_C(2),
            .access = LIB_REGMAP_ACCESS_READ_ONLY,
            .snapshot = &read_snapshot,
        },
        {
            .first_address = UINT16_C(0x0200),
            .length = UINT16_C(2),
            .access = LIB_REGMAP_ACCESS_READ_WRITE,
            .snapshot = &control_snapshot,
            .queue_write = lib_regmap_command_queue_enqueue,
            .context = &command_queue,
        },
        {
            .first_address = UINT16_C(0xfffe),
            .length = UINT16_C(2),
            .access = LIB_REGMAP_ACCESS_READ_ONLY,
            .snapshot = &wrap_snapshot,
        },
    };
    lib_regmap_command_t command = {0};
    lib_regmap_t regmap = {0};

    assert(lib_regmap_snapshot_init(&read_snapshot, read_zero, read_one,
                                    (uint16_t)sizeof(read_zero), read_zero));
    assert(lib_regmap_snapshot_init(&control_snapshot, control_zero, control_one,
                                    (uint16_t)sizeof(control_zero), control_zero));
    assert(lib_regmap_snapshot_init(&wrap_snapshot, wrap_zero, wrap_one,
                                    (uint16_t)sizeof(wrap_zero), wrap_zero));
    assert(lib_regmap_command_queue_init(
        &command_queue, command_storage,
        (uint32_t)(sizeof(command_storage) / sizeof(command_storage[0]))));
    assert(
        lib_regmap_init(&regmap, pages, (uint16_t)(sizeof(pages) / sizeof(pages[0]))));

    lib_regmap_set_address(&regmap, UINT16_C(0x0002));
    lib_regmap_begin_read(&regmap);
    assert(lib_regmap_read_current(&regmap) == LIB_REGMAP_UNMAPPED_VALUE);
    lib_regmap_end_read(&regmap);
    assert(lib_regmap_current_address(&regmap) == UINT16_C(0x0003));

    lib_regmap_set_address(&regmap, UINT16_C(0x0000));
    assert(lib_regmap_write_current(&regmap, UINT8_C(0xaa)) ==
           LIB_REGMAP_WRITE_IGNORED);
    assert(lib_regmap_current_address(&regmap) == UINT16_C(0x0001));

    lib_regmap_set_address(&regmap, UINT16_C(0x0200));
    assert(lib_regmap_write_current(&regmap, UINT8_C(0x51)) == LIB_REGMAP_WRITE_QUEUED);
    assert(lib_regmap_write_current(&regmap, UINT8_C(0x52)) == LIB_REGMAP_WRITE_QUEUED);
    assert(lib_regmap_command_queue_try_pop(&command_queue, &command));
    assert(command.address == UINT16_C(0x0200));
    assert(command.value == UINT8_C(0x51));
    assert(lib_regmap_command_queue_try_pop(&command_queue, &command));
    assert(command.address == UINT16_C(0x0201));
    assert(command.value == UINT8_C(0x52));

    assert(lib_regmap_command_queue_enqueue(&command_queue, UINT16_C(0x0200),
                                            UINT8_C(0x61)));
    assert(lib_regmap_command_queue_enqueue(&command_queue, UINT16_C(0x0201),
                                            UINT8_C(0x62)));
    assert(lib_regmap_command_queue_enqueue(&command_queue, UINT16_C(0x0202),
                                            UINT8_C(0x63)));
    assert(lib_regmap_command_queue_enqueue(&command_queue, UINT16_C(0x0203),
                                            UINT8_C(0x64)));
    lib_regmap_set_address(&regmap, UINT16_C(0x0200));
    assert(lib_regmap_write_current(&regmap, UINT8_C(0x65)) ==
           LIB_REGMAP_WRITE_REJECTED);
    assert(lib_regmap_current_address(&regmap) == UINT16_C(0x0201));
    assert(lib_regmap_command_queue_dropped_count(&command_queue) == UINT32_C(1));

    lib_regmap_set_address(&regmap, UINT16_C(0x0000));
    lib_regmap_begin_read(&regmap);
    assert(lib_regmap_read_current(&regmap) == UINT8_C(0x10));
    assert(lib_regmap_snapshot_publish(&read_snapshot, updated_read,
                                       (uint16_t)sizeof(updated_read)));
    assert(!lib_regmap_snapshot_publish(&read_snapshot, later_read,
                                        (uint16_t)sizeof(later_read)));
    assert(lib_regmap_read_current(&regmap) == UINT8_C(0x11));
    lib_regmap_end_read(&regmap);

    lib_regmap_set_address(&regmap, UINT16_C(0x0000));
    lib_regmap_begin_read(&regmap);
    assert(lib_regmap_read_current(&regmap) == UINT8_C(0x30));
    assert(lib_regmap_read_current(&regmap) == UINT8_C(0x31));
    lib_regmap_end_read(&regmap);
    assert(lib_regmap_snapshot_publish(&read_snapshot, later_read,
                                       (uint16_t)sizeof(later_read)));

    lib_regmap_set_address(&regmap, UINT16_C(0xfffe));
    lib_regmap_begin_read(&regmap);
    assert(lib_regmap_read_current(&regmap) == UINT8_C(0xfe));
    assert(lib_regmap_read_current(&regmap) == UINT8_C(0xff));
    assert(lib_regmap_current_address(&regmap) == UINT16_C(0x0000));
    assert(lib_regmap_read_current(&regmap) == UINT8_C(0x40));
    lib_regmap_end_read(&regmap);
}

static void test_crash_record_rejects_garbage_and_preserves_fault(void) {
    lib_crash_record_t record = {0};
    lib_crash_snapshot_t snapshot = {0};
    uint8_t register_image[LIB_CRASH_REGISTER_IMAGE_SIZE] = {0};

    assert(!lib_crash_is_valid(&record));
    assert(!lib_crash_has_fault(&record));
    assert(!lib_crash_decode(&record, &snapshot));
    assert(snapshot.reason == LIB_CRASH_REASON_NONE);
    assert(!lib_crash_write_register_image(&record, register_image,
                                           (uint32_t)sizeof(register_image)));
    assert(register_image[0] == 0U);
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
    assert(lib_crash_decode(&record, &snapshot));
    assert(snapshot.reason == LIB_CRASH_REASON_HARDFAULT);
    assert(snapshot.sequence == UINT32_C(1));
    assert(snapshot.reset_cause == UINT32_C(0x0D));
    assert(snapshot.exception_number == UINT32_C(3));
    assert(snapshot.stacked_pc == UINT32_C(0x12345678));
    assert(snapshot.stacked_xpsr == UINT32_C(0x21000003));
    assert(!lib_crash_write_register_image(
        &record, register_image, LIB_CRASH_REGISTER_IMAGE_SIZE - UINT32_C(1)));
    assert(lib_crash_write_register_image(&record, register_image,
                                          (uint32_t)sizeof(register_image)));
    assert(register_image[0] == UINT8_C(1));
    assert(register_image[1] == (uint8_t)LIB_CRASH_REASON_HARDFAULT);
    assert(register_image[2] == 0U);
    assert(register_image[3] == LIB_CRASH_FORMAT_VERSION);
    assert(register_image[4] == 0U);
    assert(register_image[7] == UINT8_C(1));
    assert(register_image[8] == 0U);
    assert(register_image[11] == UINT8_C(0x0D));
    assert(register_image[12] == 0U);
    assert(register_image[15] == UINT8_C(3));
    assert(register_image[16] == UINT8_C(0x12));
    assert(register_image[17] == UINT8_C(0x34));
    assert(register_image[18] == UINT8_C(0x56));
    assert(register_image[19] == UINT8_C(0x78));
    assert(register_image[20] == UINT8_C(0x21));
    assert(register_image[21] == 0U);
    assert(register_image[22] == 0U);
    assert(register_image[23] == UINT8_C(3));

    lib_crash_note_boot(&record, UINT32_C(0x0E));
    assert(lib_crash_is_valid(&record));
    assert(lib_crash_has_fault(&record));
    assert(record.sequence == UINT32_C(1));
    assert(record.reset_cause == UINT32_C(0x0E));

    record.stacked_pc ^= UINT32_C(1);
    assert(!lib_crash_is_valid(&record));
    assert(!lib_crash_has_fault(&record));
    assert(!lib_crash_decode(&record, &snapshot));
    assert(snapshot.reason == LIB_CRASH_REASON_NONE);
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

    lib_crash_capture(&record, (lib_crash_reason_t)UINT32_C(0xff), UINT32_C(0),
                      UINT32_C(0), UINT32_C(0));
    assert(lib_crash_is_valid(&record));
    assert(lib_crash_reason(&record) == LIB_CRASH_REASON_UNEXPECTED_EXCEPTION);
}

int main(void) {
    _Static_assert(sizeof(uint32_t) == 4U, "uint32_t must be exactly 32 bits");
    _Static_assert(sizeof(uint8_t) == 1U, "uint8_t must be exactly 8 bits");

    test_rejects_invalid_storage_and_capacity();
    test_empty_and_null_operations();
    test_full_queue_preserves_fifo_data_and_counts_drop();
    test_wraparound_and_index_overflow();
    test_debug_transport_is_register_free_callback();
    test_buildinfo_and_boot_banner();
    test_regmap_command_queue();
    test_regmap_rejects_invalid_page_layout();
    test_regmap_access_rules_and_snapshot_latching();
    test_crash_record_rejects_garbage_and_preserves_fault();
    test_crash_record_restarts_sequence_after_invalid_record();
    return 0;
}
