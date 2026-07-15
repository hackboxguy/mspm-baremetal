#include <stdint.h>

#include "arch_wait.h"
#include "board.h"
#include "hal_i2c_target.h"
#include "hal_timer.h"
#include "hal_wdt.h"
#include "lib_boot.h"
#include "lib_buildinfo.h"
#include "lib_crash.h"
#include "lib_debug.h"
#include "lib_regmap.h"

#define I2C_REGMAP_DEMO_STATUS_SIZE UINT16_C(12)
#define I2C_REGMAP_DEMO_STATUS_ERROR_COUNT_OFFSET UINT16_C(0)
#define I2C_REGMAP_DEMO_STATUS_PUBLISH_REJECTED_OFFSET UINT16_C(4)
#define I2C_REGMAP_DEMO_STATUS_UNEXPECTED_EVENT_OFFSET UINT16_C(8)

extern lib_crash_record_t g_crash_record;

static uint8_t g_device_info_zero[LIB_BUILDINFO_DEVICE_INFO_SIZE];
static uint8_t g_device_info_one[LIB_BUILDINFO_DEVICE_INFO_SIZE];
static uint8_t g_status_zero[I2C_REGMAP_DEMO_STATUS_SIZE];
static uint8_t g_status_one[I2C_REGMAP_DEMO_STATUS_SIZE];
static uint8_t g_crash_zero[LIB_CRASH_REGISTER_IMAGE_SIZE];
static uint8_t g_crash_one[LIB_CRASH_REGISTER_IMAGE_SIZE];
static lib_regmap_snapshot_t g_device_info_snapshot;
static lib_regmap_snapshot_t g_status_snapshot;
static lib_regmap_snapshot_t g_crash_snapshot;
static uint32_t g_status_last_error_count;
static uint32_t g_status_last_publish_rejected_count;
static uint32_t g_status_last_unexpected_event_count;
static bool g_status_published;

static const lib_regmap_page_t g_pages[] = {
    {
        .first_address = UINT16_C(0x0000),
        .length = LIB_BUILDINFO_DEVICE_INFO_SIZE,
        .access = LIB_REGMAP_ACCESS_READ_ONLY,
        .snapshot = &g_device_info_snapshot,
    },
    {
        .first_address = UINT16_C(0x0300),
        .length = I2C_REGMAP_DEMO_STATUS_SIZE,
        .access = LIB_REGMAP_ACCESS_READ_ONLY,
        .snapshot = &g_status_snapshot,
    },
    {
        .first_address = UINT16_C(0x0400),
        .length = LIB_CRASH_REGISTER_IMAGE_SIZE,
        .access = LIB_REGMAP_ACCESS_READ_ONLY,
        .snapshot = &g_crash_snapshot,
    },
};

static lib_regmap_t g_regmap;

static const lib_buildinfo_t g_buildinfo = {
    .version_major = (uint8_t)FW_VERSION_MAJOR,
    .version_minor = (uint8_t)FW_VERSION_MINOR,
};

static void i2c_regmap_demo_write_u32_be(uint8_t *bytes, uint16_t offset,
                                         uint32_t value) {
    bytes[offset] = (uint8_t)(value >> 24U);
    bytes[offset + UINT16_C(1)] = (uint8_t)(value >> 16U);
    bytes[offset + UINT16_C(2)] = (uint8_t)(value >> 8U);
    bytes[offset + UINT16_C(3)] = (uint8_t)value;
}

static bool i2c_regmap_demo_init(void) {
    uint8_t crash_image[LIB_CRASH_REGISTER_IMAGE_SIZE];

    if (!lib_buildinfo_write_device_info(&g_buildinfo, g_device_info_zero,
                                         (uint32_t)sizeof(g_device_info_zero)) ||
        !lib_regmap_snapshot_init(
            &g_device_info_snapshot, g_device_info_zero, g_device_info_one,
            (uint16_t)sizeof(g_device_info_zero), g_device_info_zero)) {
        return false;
    }

    (void)lib_crash_write_register_image(&g_crash_record, crash_image,
                                         (uint32_t)sizeof(crash_image));
    if (!lib_regmap_snapshot_init(&g_crash_snapshot, g_crash_zero, g_crash_one,
                                  (uint16_t)sizeof(g_crash_zero), crash_image) ||
        !lib_regmap_snapshot_init(&g_status_snapshot, g_status_zero, g_status_one,
                                  (uint16_t)sizeof(g_status_zero), g_status_zero) ||
        !lib_regmap_init(&g_regmap, g_pages,
                         (uint16_t)(sizeof(g_pages) / sizeof(g_pages[0])))) {
        return false;
    }

    return board_i2c1_target_init(&g_regmap);
}

static void i2c_regmap_demo_publish_status(void) {
    uint8_t status[I2C_REGMAP_DEMO_STATUS_SIZE] = {0};
    const uint32_t error_count = hal_i2c_target_error_count();
    const uint32_t publish_rejected_count =
        lib_regmap_snapshot_publish_rejected_count(&g_status_snapshot);
    const uint32_t unexpected_event_count = hal_i2c_target_unexpected_event_count();

    if (g_status_published && (error_count == g_status_last_error_count) &&
        (publish_rejected_count == g_status_last_publish_rejected_count) &&
        (unexpected_event_count == g_status_last_unexpected_event_count)) {
        return;
    }

    i2c_regmap_demo_write_u32_be(status, I2C_REGMAP_DEMO_STATUS_ERROR_COUNT_OFFSET,
                                 error_count);
    i2c_regmap_demo_write_u32_be(status, I2C_REGMAP_DEMO_STATUS_PUBLISH_REJECTED_OFFSET,
                                 publish_rejected_count);
    i2c_regmap_demo_write_u32_be(status, I2C_REGMAP_DEMO_STATUS_UNEXPECTED_EVENT_OFFSET,
                                 unexpected_event_count);
    if (lib_regmap_snapshot_publish(&g_status_snapshot, status,
                                    (uint16_t)sizeof(status))) {
        g_status_last_error_count = error_count;
        g_status_last_publish_rejected_count = publish_rejected_count;
        g_status_last_unexpected_event_count = unexpected_event_count;
        g_status_published = true;
    }
}

static void i2c_regmap_demo_fail(void) {
    board_led_red_set(true);
    for (;;) {
        arch_wait_for_interrupt();
    }
}

int main(void) {
    const board_init_result_t init_result = board_init();

    if ((init_result == BOARD_INIT_GPIO_FAILURE) ||
        (init_result == BOARD_INIT_UART_FAILURE) || !hal_timer_init_1ms() ||
        !i2c_regmap_demo_init() || !hal_wdt_init()) {
        i2c_regmap_demo_fail();
    }

    DBG_WRITE_LITERAL("mspm-baremetal: i2c_regmap_demo\r\n");
    for (;;) {
        i2c_regmap_demo_publish_status();
        hal_wdt_kick();
        arch_wait_for_interrupt();
    }
}
