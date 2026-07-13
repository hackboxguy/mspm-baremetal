#ifndef MSPM_HAL_UART_H
#define MSPM_HAL_UART_H

#include <stdbool.h>
#include <stdint.h>

#include "lib_ringbuf.h"

typedef struct {
    uint32_t clock_hz;
    uint32_t baud_rate;
    uint32_t tx_pincm_index;
    uint32_t tx_pincm_function;
} hal_uart0_tx_config_t;

/*
 * Initializes the single UART0 TX path. The caller owns the queue storage and
 * is its sole producer; UART0_IRQHandler is the sole consumer. RX is not
 * configured or enabled by this TX-only HAL.
 */
bool hal_uart0_tx_init(const hal_uart0_tx_config_t *config, lib_ringbuf_t *queue);

/* Returns the number of bytes accepted. A full queue increments its drop count. */
uint32_t hal_uart0_tx_write(const uint8_t *data, uint32_t length);
uint32_t hal_uart0_tx_dropped_count(void);

#endif
