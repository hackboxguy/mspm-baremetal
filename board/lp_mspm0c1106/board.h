#ifndef MSPM_BOARD_LP_MSPM0C1106_H
#define MSPM_BOARD_LP_MSPM0C1106_H

#include <stdbool.h>
#include <stdint.h>

#include "lib_regmap.h"

#define BOARD_NAME "lp_mspm0c1106"

/*
 * LP-MSPM0C1106 user LED1 is active-high GPIOA DIO0 / PINCM1.  This board
 * owns that pin; applications use the board LED functions instead of GPIO.
 */
#define BOARD_LED_RED_PIN_INDEX UINT32_C(0)
#define BOARD_LED_RED_MASK (UINT32_C(1) << BOARD_LED_RED_PIN_INDEX)

/* SYSOSC base frequency and the user-visible blink cadence. */
#define BOARD_MCLK_HZ UINT32_C(32000000)
#define BOARD_BLINK_PERIOD_MS UINT32_C(500)
#define BOARD_BLINK_FALLBACK_PERIOD_MS UINT32_C(125)
#define BOARD_UART_BACKCHANNEL_BAUD UINT32_C(115200)
#define BOARD_I2C_REGMAP_TARGET_ADDRESS UINT8_C(0x42)
#define BOARD_I2C_CONTROLLER_BUS_HZ UINT32_C(100000)
#define BOARD_I2C_CONTROLLER_POLL_LIMIT UINT32_C(32000)
#define BOARD_I2C_CONTROLLER_SCL_LOW_TIMEOUT_COUNT UINT8_C(32)

typedef enum {
    BOARD_INIT_OK,
    BOARD_INIT_CLOCK_FALLBACK,
    BOARD_INIT_GPIO_FAILURE,
    BOARD_INIT_UART_FAILURE,
} board_init_result_t;

/* Initializes the run clock and red LED GPIO, reporting either degraded path. */
board_init_result_t board_init(void);
void board_led_red_set(bool on);
void board_led_red_toggle(void);
uint32_t board_uart_backchannel_dropped_count(void);
bool board_crash_has_fault(void);
bool board_i2c1_target_init(lib_regmap_t *regmap);
bool board_i2c1_controller_init(void);

#endif
