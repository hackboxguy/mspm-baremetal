#ifndef MSPM_BOARD_LP_MSPM0C1106_H
#define MSPM_BOARD_LP_MSPM0C1106_H

#include <stdbool.h>
#include <stdint.h>

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

/* Returns false only when the requested run clock fell back to safe 4 MHz. */
bool board_init(void);
void board_led_red_set(bool on);
void board_led_red_toggle(void);

#endif
