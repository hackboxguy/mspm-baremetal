#include <stdint.h>

#include "arch_wait.h"
#include "board.h"
#include "hal_i2c_controller.h"
#include "hal_timer.h"
#include "hal_wdt.h"
#include "lib_debug.h"

#define PCF8574A_ADDRESS_FIRST UINT8_C(0x38)
#define PCF8574A_ADDRESS_LAST UINT8_C(0x3f)
#define PCF8574_ADDRESS_FIRST UINT8_C(0x20)
#define PCF8574_ADDRESS_LAST UINT8_C(0x27)
#define PCF8574A_RELEASE_ALL_PORTS UINT8_C(0xff)

typedef enum {
    PCF8574A_DEMO_STAGE_INIT,
    PCF8574A_DEMO_STAGE_SCAN,
    PCF8574A_DEMO_STAGE_SEPARATE_READ,
    PCF8574A_DEMO_STAGE_COMBINED_READ,
    PCF8574A_DEMO_STAGE_COMPLETE,
} pcf8574a_demo_stage_t;

/* Kept visible for SWD inspection in a release probe image. */
volatile uint8_t g_pcf8574a_address;
volatile uint8_t g_pcf8574a_port_value;
volatile hal_i2c_controller_result_t g_pcf8574a_result;
volatile pcf8574a_demo_stage_t g_pcf8574a_stage;
static bool g_watchdog_running;

static void pcf8574a_demo_fail(hal_i2c_controller_result_t result) {
    g_pcf8574a_result = result;
    board_led_red_set(true);
    for (;;) {
        if (g_watchdog_running) {
            hal_wdt_kick();
        }
        arch_wait_for_interrupt();
    }
}

static bool pcf8574a_demo_try_range(uint8_t first_address, uint8_t last_address) {
    const uint8_t release_ports = PCF8574A_RELEASE_ALL_PORTS;
    uint8_t address;

    for (address = first_address; address <= last_address; ++address) {
        const hal_i2c_controller_result_t result =
            hal_i2c_controller_write(address, &release_ports, UINT8_C(1));

        if (result == HAL_I2C_CONTROLLER_OK) {
            g_pcf8574a_address = address;
            return true;
        }
        if (result != HAL_I2C_CONTROLLER_ADDRESS_NACK) {
            g_pcf8574a_result = result;
            return false;
        }
    }

    return true;
}

static bool pcf8574a_demo_find_target(void) {
    if (!pcf8574a_demo_try_range(PCF8574A_ADDRESS_FIRST, PCF8574A_ADDRESS_LAST)) {
        return false;
    }
    if (g_pcf8574a_address != 0U) {
        return true;
    }
    if (!pcf8574a_demo_try_range(PCF8574_ADDRESS_FIRST, PCF8574_ADDRESS_LAST)) {
        return false;
    }
    if (g_pcf8574a_address != 0U) {
        return true;
    }

    g_pcf8574a_result = HAL_I2C_CONTROLLER_ADDRESS_NACK;
    return false;
}

int main(void) {
    const board_init_result_t init_result = board_init();
    const uint8_t release_ports = PCF8574A_RELEASE_ALL_PORTS;
    uint8_t port_value;
    uint32_t next_toggle_ms;

    g_pcf8574a_address = 0U;
    g_pcf8574a_port_value = 0U;
    g_pcf8574a_result = HAL_I2C_CONTROLLER_NOT_INITIALIZED;
    g_pcf8574a_stage = PCF8574A_DEMO_STAGE_INIT;
    g_watchdog_running = false;

    if ((init_result == BOARD_INIT_GPIO_FAILURE) ||
        (init_result == BOARD_INIT_UART_FAILURE) || !hal_timer_init_1ms() ||
        !board_i2c1_controller_init() || !hal_wdt_init()) {
        pcf8574a_demo_fail(HAL_I2C_CONTROLLER_NOT_INITIALIZED);
    }
    g_watchdog_running = true;

    DBG_WRITE_LITERAL("mspm-baremetal: pcf8574a_demo\r\n");
    g_pcf8574a_stage = PCF8574A_DEMO_STAGE_SCAN;
    if (!pcf8574a_demo_find_target()) {
        pcf8574a_demo_fail(g_pcf8574a_result);
    }

    g_pcf8574a_stage = PCF8574A_DEMO_STAGE_SEPARATE_READ;
    g_pcf8574a_result =
        hal_i2c_controller_read(g_pcf8574a_address, &port_value, UINT8_C(1));
    if (g_pcf8574a_result != HAL_I2C_CONTROLLER_OK) {
        pcf8574a_demo_fail(g_pcf8574a_result);
    }
    g_pcf8574a_port_value = port_value;

    g_pcf8574a_stage = PCF8574A_DEMO_STAGE_COMBINED_READ;
    g_pcf8574a_result = hal_i2c_controller_write_read(
        g_pcf8574a_address, &release_ports, UINT8_C(1), &port_value, UINT8_C(1));
    if (g_pcf8574a_result != HAL_I2C_CONTROLLER_OK) {
        pcf8574a_demo_fail(g_pcf8574a_result);
    }
    g_pcf8574a_port_value = port_value;

    g_pcf8574a_stage = PCF8574A_DEMO_STAGE_COMPLETE;
    DBG_WRITE_LITERAL("PCF8574A_COMBINED_OK\r\n");
    next_toggle_ms = hal_timer_now_ms() + BOARD_BLINK_PERIOD_MS;
    for (;;) {
        if (hal_timer_deadline_reached(next_toggle_ms)) {
            board_led_red_toggle();
            next_toggle_ms = hal_timer_now_ms() + BOARD_BLINK_PERIOD_MS;
        }
        hal_wdt_kick();
        arch_wait_for_interrupt();
    }
}
