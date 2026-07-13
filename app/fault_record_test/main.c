#include "board.h"
#include "hal_timer.h"
#include "hal_wdt.h"
#include "lib_debug.h"

static void fault_record_test_wait_healthy(void) {
    for (;;) {
        hal_wdt_kick();
        hal_timer_wait_for_interrupt();
    }
}

int main(void) {
    const board_init_result_t init_result = board_init();

    if ((init_result == BOARD_INIT_GPIO_FAILURE) ||
        (init_result == BOARD_INIT_UART_FAILURE) || !hal_timer_init_1ms() ||
        !hal_wdt_init()) {
        board_led_red_set(true);
        for (;;) {
            hal_timer_wait_for_interrupt();
        }
    }

    if (board_crash_has_fault()) {
        DBG_WRITE_LITERAL("mspm-baremetal: retained hard fault\r\n");
        fault_record_test_wait_healthy();
    }

    DBG_WRITE_LITERAL("mspm-baremetal: triggering hard fault\r\n");
    __asm volatile("udf #0");

    fault_record_test_wait_healthy();
}
