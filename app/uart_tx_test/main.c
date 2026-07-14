#include <stdint.h>

#include "arch_wait.h"
#include "board.h"
#include "hal_timer.h"
#include "hal_uart.h"
#include "hal_wdt.h"
#include "lib_debug.h"

#define UART_TX_TEST_DRAIN_WAIT_MS UINT32_C(50)
#define UART_TX_TEST_BLINK_PERIOD_MS UINT32_C(500)
#define UART_TX_TEST_BLOCK "0123456789ABCDEF"

static const uint8_t g_lossless_burst[] = "0123456789ABCDEF0123456789ABCDEF";
static const uint8_t g_overload_burst[] =
    UART_TX_TEST_BLOCK UART_TX_TEST_BLOCK UART_TX_TEST_BLOCK UART_TX_TEST_BLOCK
        UART_TX_TEST_BLOCK UART_TX_TEST_BLOCK UART_TX_TEST_BLOCK UART_TX_TEST_BLOCK;

_Static_assert((sizeof(g_lossless_burst) - 1U) == 32U,
               "lossless UART test burst must remain 32 bytes");
_Static_assert((sizeof(g_overload_burst) - 1U) == 128U,
               "overload UART test burst must remain 128 bytes");

static void uart_tx_test_wait_ms(uint32_t delay_ms) {
    const uint32_t deadline = hal_timer_now_ms() + delay_ms;

    while (!hal_timer_deadline_reached(deadline)) {
        hal_wdt_kick();
        arch_wait_for_interrupt();
    }
}

static void uart_tx_test_fail(void) {
    board_led_red_set(true);
    for (;;) {
        hal_wdt_kick();
        arch_wait_for_interrupt();
    }
}

int main(void) {
    const board_init_result_t init_result = board_init();
    uint32_t accepted;
    uint32_t drop_count_before;
    uint32_t next_toggle_ms;

    if ((init_result == BOARD_INIT_GPIO_FAILURE) ||
        (init_result == BOARD_INIT_UART_FAILURE) || !hal_timer_init_1ms() ||
        !hal_wdt_init()) {
        uart_tx_test_fail();
    }

    DBG_WRITE_LITERAL("mspm-baremetal: uart_tx_test\r\n");
    uart_tx_test_wait_ms(UART_TX_TEST_DRAIN_WAIT_MS);

    drop_count_before = board_uart_backchannel_dropped_count();
    accepted =
        hal_uart0_tx_write(g_lossless_burst, (uint32_t)(sizeof(g_lossless_burst) - 1U));
    if ((accepted != (uint32_t)(sizeof(g_lossless_burst) - 1U)) ||
        (board_uart_backchannel_dropped_count() != drop_count_before)) {
        uart_tx_test_fail();
    }

    uart_tx_test_wait_ms(UART_TX_TEST_DRAIN_WAIT_MS);
    DBG_WRITE_LITERAL("UART_TX_LOSSLESS_OK\r\n");
    uart_tx_test_wait_ms(UART_TX_TEST_DRAIN_WAIT_MS);

    drop_count_before = board_uart_backchannel_dropped_count();
    accepted =
        hal_uart0_tx_write(g_overload_burst, (uint32_t)(sizeof(g_overload_burst) - 1U));
    if ((accepted >= (uint32_t)(sizeof(g_overload_burst) - 1U)) ||
        (board_uart_backchannel_dropped_count() == drop_count_before)) {
        uart_tx_test_fail();
    }

    uart_tx_test_wait_ms(UART_TX_TEST_DRAIN_WAIT_MS);
    DBG_WRITE_LITERAL("UART_TX_OVERFLOW_OK\r\n");

    next_toggle_ms = hal_timer_now_ms() + UART_TX_TEST_BLINK_PERIOD_MS;
    for (;;) {
        if (hal_timer_deadline_reached(next_toggle_ms)) {
            board_led_red_toggle();
            next_toggle_ms = hal_timer_now_ms() + UART_TX_TEST_BLINK_PERIOD_MS;
        }
        hal_wdt_kick();
        arch_wait_for_interrupt();
    }
}
