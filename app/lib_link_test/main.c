#include <stdint.h>

#include "arch_wait.h"
#include "lib_boot.h"
#include "lib_buildinfo.h"
#include "lib_crash.h"
#include "lib_debug.h"
#include "lib_regmap.h"
#include "lib_ringbuf.h"

static volatile uint32_t g_link_test_sink;

static uint32_t link_test_writer(const uint8_t *data, uint32_t length) {
    (void)data;
    return length;
}

static void link_test_reference_all_libraries(void) {
    const lib_buildinfo_t buildinfo = {
        .version_major = UINT8_C(0x01),
        .version_minor = UINT8_C(0x02),
    };
    uint8_t device_info[LIB_BUILDINFO_DEVICE_INFO_SIZE] = {0};
    uint8_t ring_storage[2] = {0};
    uint8_t byte = 0U;
    lib_ringbuf_t ring = {0};
    lib_crash_record_t crash_record = {0};
    lib_crash_snapshot_t crash_snapshot = {0};
    uint8_t crash_image[LIB_CRASH_REGISTER_IMAGE_SIZE] = {0};
    uint8_t snapshot_zero[2] = {0};
    uint8_t snapshot_one[2] = {0};
    const uint8_t snapshot_initial[2] = {UINT8_C(0x10), UINT8_C(0x11)};
    const uint8_t snapshot_update[2] = {UINT8_C(0x20), UINT8_C(0x21)};
    lib_regmap_snapshot_t snapshot = {0};
    lib_regmap_command_t command_storage[2] = {0};
    lib_regmap_command_t command = {0};
    lib_regmap_command_queue_t command_queue = {0};
    const lib_regmap_page_t page = {
        .first_address = 0U,
        .length = UINT16_C(2),
        .access = LIB_REGMAP_ACCESS_READ_WRITE,
        .snapshot = &snapshot,
        .queue_write = lib_regmap_command_queue_enqueue,
        .context = &command_queue,
    };
    lib_regmap_t regmap = {0};

    g_link_test_sink += lib_buildinfo_is_bcd(buildinfo.version_major) ? 1U : 0U;
    g_link_test_sink += lib_buildinfo_has_timestamp(&buildinfo) ? 1U : 0U;
    g_link_test_sink += lib_buildinfo_is_valid(&buildinfo) ? 1U : 0U;
    g_link_test_sink += lib_buildinfo_write_device_info(&buildinfo, device_info,
                                                        (uint32_t)sizeof(device_info))
                            ? 1U
                            : 0U;
    lib_debug_set_writer(link_test_writer);
    g_link_test_sink += lib_debug_write(device_info, (uint32_t)sizeof(device_info));
    lib_boot_write_banner("lib_link_test", &buildinfo);

    g_link_test_sink +=
        lib_ringbuf_init(&ring, ring_storage, (uint32_t)sizeof(ring_storage)) ? 1U : 0U;
    g_link_test_sink += lib_ringbuf_try_push(&ring, UINT8_C(0x5a)) ? 1U : 0U;
    g_link_test_sink += lib_ringbuf_try_pop(&ring, &byte) ? (uint32_t)byte : 0U;
    g_link_test_sink += lib_ringbuf_capacity(&ring);
    g_link_test_sink += lib_ringbuf_dropped_count(&ring);

    lib_crash_note_boot(&crash_record, UINT32_C(1));
    lib_crash_capture(&crash_record, LIB_CRASH_REASON_HARDFAULT, UINT32_C(3),
                      UINT32_C(0x100), UINT32_C(0x01000000));
    g_link_test_sink += lib_crash_is_valid(&crash_record) ? 1U : 0U;
    g_link_test_sink += lib_crash_has_fault(&crash_record) ? 1U : 0U;
    g_link_test_sink += (uint32_t)lib_crash_reason(&crash_record);
    g_link_test_sink += lib_crash_exception_number(&crash_record);
    g_link_test_sink += lib_crash_decode(&crash_record, &crash_snapshot) ? 1U : 0U;
    g_link_test_sink += lib_crash_write_register_image(&crash_record, crash_image,
                                                       (uint32_t)sizeof(crash_image))
                            ? 1U
                            : 0U;

    g_link_test_sink +=
        lib_regmap_snapshot_init(&snapshot, snapshot_zero, snapshot_one,
                                 (uint16_t)sizeof(snapshot_zero), snapshot_initial)
            ? 1U
            : 0U;
    g_link_test_sink +=
        lib_regmap_command_queue_init(
            &command_queue, command_storage,
            (uint32_t)(sizeof(command_storage) / sizeof(command_storage[0])))
            ? 1U
            : 0U;
    g_link_test_sink += lib_regmap_init(&regmap, &page, UINT16_C(1)) ? 1U : 0U;
    g_link_test_sink += lib_regmap_snapshot_publish(&snapshot, snapshot_update,
                                                    (uint16_t)sizeof(snapshot_update))
                            ? 1U
                            : 0U;
    lib_regmap_set_address(&regmap, 0U);
    lib_regmap_begin_read(&regmap);
    g_link_test_sink += lib_regmap_read_current(&regmap);
    lib_regmap_end_read(&regmap);
    g_link_test_sink += (uint32_t)lib_regmap_write_current(&regmap, UINT8_C(0x42));
    g_link_test_sink += lib_regmap_current_address(&regmap);
    g_link_test_sink += lib_regmap_command_queue_try_pop(&command_queue, &command)
                            ? (uint32_t)command.value
                            : 0U;
    g_link_test_sink += lib_regmap_command_queue_dropped_count(&command_queue);
    g_link_test_sink += lib_regmap_snapshot_publish_rejected_count(&snapshot);
}

int main(void) {
    link_test_reference_all_libraries();
    for (;;) {
        arch_wait_for_interrupt();
    }
}
