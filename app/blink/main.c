#include "board.h"
#include "hal_timer.h"

int main(void) {
    uint32_t next_toggle_ms;

    (void)board_init();

    if (!hal_timer_init_1ms()) {
        /* A steady LED is an unambiguous timer-bring-up failure indication. */
        board_led_red_set(true);
        for (;;) {
            hal_timer_wait_for_interrupt();
        }
    }

    next_toggle_ms = hal_timer_now_ms() + BOARD_BLINK_PERIOD_MS;
    for (;;) {
        if (hal_timer_deadline_reached(next_toggle_ms)) {
            board_led_red_toggle();
            next_toggle_ms = hal_timer_now_ms() + BOARD_BLINK_PERIOD_MS;
        }

        hal_timer_wait_for_interrupt();
    }
}
