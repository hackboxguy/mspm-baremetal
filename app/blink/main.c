#include "board.h"
#include "hal_timer.h"
#include "hal_wdt.h"
#include "lib_debug.h"

int main(void) {
    const board_init_result_t init_result = board_init();
    uint32_t blink_period_ms = BOARD_BLINK_PERIOD_MS;
    uint32_t next_toggle_ms;

    if ((init_result == BOARD_INIT_GPIO_FAILURE) ||
        (init_result == BOARD_INIT_UART_FAILURE)) {
        /* Best effort: a steady LED distinguishes a board-init failure. */
        board_led_red_set(true);
        for (;;) {
            hal_timer_wait_for_interrupt();
        }
    }

    if (init_result == BOARD_INIT_CLOCK_FALLBACK) {
        /* Four fast blinks per normal interval make the safe clock observable. */
        blink_period_ms = BOARD_BLINK_FALLBACK_PERIOD_MS;
    }

    if (!hal_timer_init_1ms()) {
        /* A steady LED is an unambiguous timer-bring-up failure indication. */
        board_led_red_set(true);
        for (;;) {
            hal_timer_wait_for_interrupt();
        }
    }

    DBG_WRITE_LITERAL("mspm-baremetal: blink\r\n");
    if (!hal_wdt_init()) {
        board_led_red_set(true);
        for (;;) {
            hal_timer_wait_for_interrupt();
        }
    }

    next_toggle_ms = hal_timer_now_ms() + blink_period_ms;
    for (;;) {
        if (hal_timer_deadline_reached(next_toggle_ms)) {
            board_led_red_toggle();
            next_toggle_ms = hal_timer_now_ms() + blink_period_ms;
        }

        hal_wdt_kick();
        hal_timer_wait_for_interrupt();
    }
}
