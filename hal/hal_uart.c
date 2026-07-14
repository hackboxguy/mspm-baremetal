#include "hal_uart.h"

#include <stddef.h>

#include "arch_critical.h"
#include "device.h"
#include "hal_power.h"

#define HAL_UART_PINCM_COUNT UINT32_C(251)
#define HAL_UART_TX_PRIORITY UINT32_C(2)
#define HAL_UART_DIVISOR_FRACTION_SCALE UINT32_C(64)

static lib_ringbuf_t *g_uart0_tx_queue;
static bool g_uart0_tx_initialized;

static bool hal_uart_baud_divisor(uint32_t clock_hz, uint32_t baud_rate,
                                  uint32_t *divisor) {
    uint64_t scaled_divisor;

    if ((baud_rate == 0U) || (divisor == NULL)) {
        return false;
    }

    /*
     * For 16x oversampling, the 6-bit fractional divisor is
     * round((clock_hz * 4) / baud_rate). This is the TI DriverLib formula,
     * written in 64-bit arithmetic to keep the calculation general.
     */
    scaled_divisor =
        (((uint64_t)clock_hz * UINT64_C(8)) / baud_rate + UINT64_C(1)) / UINT64_C(2);
    if ((scaled_divisor < HAL_UART_DIVISOR_FRACTION_SCALE) ||
        (scaled_divisor > UINT64_C(0x003fffff))) {
        return false;
    }

    *divisor = (uint32_t)scaled_divisor;
    return true;
}

static void hal_uart0_tx_enable_interrupt(void) {
    const arch_critical_state_t state = arch_critical_enter();

    UART0->CPU_INT.IMASK |= UART_CPU_INT_IMASK_TXINT_SET;
    /* Kick the first transfer; later requests are FIFO-level driven. */
    UART0->CPU_INT.ISET = UART_CPU_INT_ISET_TXINT_SET;
    arch_critical_exit(state);
}

static void hal_uart0_tx_disable_interrupt(void) {
    const arch_critical_state_t state = arch_critical_enter();

    UART0->CPU_INT.IMASK &= ~UART_CPU_INT_IMASK_TXINT_MASK;
    arch_critical_exit(state);
}

static void hal_uart0_tx_service(void) {
    bool queue_empty = false;
    uint8_t byte;

    while ((UART0->STAT & UART_STAT_TXFF_MASK) == 0U) {
        if (!lib_ringbuf_try_pop(g_uart0_tx_queue, &byte)) {
            queue_empty = true;
            break;
        }

        UART0->TXDATA = byte;
    }

    if (queue_empty) {
        hal_uart0_tx_disable_interrupt();
    }
}

bool hal_uart0_tx_init(const hal_uart0_tx_config_t *config, lib_ringbuf_t *queue) {
    uint32_t divisor;

    if ((config == NULL) || (queue == NULL) || (config->clock_hz == 0U) ||
        (config->tx_pincm_index >= HAL_UART_PINCM_COUNT) ||
        ((config->tx_pincm_function & ~IOMUX_PINCM_PF_MASK) != 0U) ||
        (lib_ringbuf_capacity(queue) == 0U) ||
        !hal_uart_baud_divisor(config->clock_hz, config->baud_rate, &divisor)) {
        return false;
    }

    UART0->GPRCM.RSTCTL = UART_RSTCTL_KEY_UNLOCK_W | UART_RSTCTL_RESETSTKYCLR_CLR |
                          UART_RSTCTL_RESETASSERT_ASSERT;
    UART0->GPRCM.PWREN = UART_PWREN_KEY_UNLOCK_W | UART_PWREN_ENABLE_ENABLE;
    hal_power_wait_after_enable();

    IOMUX->SECCFG.PINCM[config->tx_pincm_index] =
        config->tx_pincm_function | IOMUX_PINCM_PC_CONNECTED;

    UART0->CTL0 = UART_CTL0_ENABLE_DISABLE;
    UART0->CLKSEL = UART_CLKSEL_BUSCLK_SEL_ENABLE;
    UART0->CLKDIV = UART_CLKDIV_RATIO_DIV_BY_1;
    UART0->LCRH = UART_LCRH_WLEN_DATABIT8;
    UART0->IBRD = divisor / HAL_UART_DIVISOR_FRACTION_SCALE;
    UART0->FBRD = divisor & (HAL_UART_DIVISOR_FRACTION_SCALE - UINT32_C(1));
    UART0->IFLS = UART_IFLS_TXIFLSEL_LVL_1_2;
    UART0->CPU_INT.ICLR = UART_CPU_INT_ICLR_TXINT_CLR;
    UART0->CPU_INT.IMASK = 0U;
    UART0->CTL0 = UART_CTL0_ENABLE_ENABLE | UART_CTL0_TXE_ENABLE |
                  UART_CTL0_FEN_ENABLE | UART_CTL0_HSE_OVS16;

    g_uart0_tx_queue = queue;
    g_uart0_tx_initialized = true;
    NVIC_SetPriority(UART0_INT_IRQn, HAL_UART_TX_PRIORITY);
    NVIC_ClearPendingIRQ(UART0_INT_IRQn);
    NVIC_EnableIRQ(UART0_INT_IRQn);
    return true;
}

uint32_t hal_uart0_tx_write(const uint8_t *data, uint32_t length) {
    uint32_t accepted = 0U;

    if (!g_uart0_tx_initialized || ((data == NULL) && (length != 0U))) {
        return 0U;
    }

    while (accepted < length) {
        if (!lib_ringbuf_try_push(g_uart0_tx_queue, data[accepted])) {
            break;
        }

        ++accepted;
    }

    if (accepted != 0U) {
        hal_uart0_tx_enable_interrupt();
    }

    return accepted;
}

uint32_t hal_uart0_tx_dropped_count(void) {
    return g_uart0_tx_initialized ? lib_ringbuf_dropped_count(g_uart0_tx_queue) : 0U;
}

void UART0_IRQHandler(void) {
    const uint32_t interrupt = UART0->CPU_INT.IIDX & UART_CPU_INT_IIDX_STAT_MASK;

    if (interrupt == UART_CPU_INT_IIDX_STAT_TXIFG) {
        hal_uart0_tx_service();
    }
}
