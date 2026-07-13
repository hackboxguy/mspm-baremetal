#ifndef MSPM_HAL_CLOCK_H
#define MSPM_HAL_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#define HAL_CLOCK_SAFE_HZ UINT32_C(4000000)
#define HAL_CLOCK_MSPM0C1106_MAX_HZ UINT32_C(32000000)

/*
 * Selects SYSOSC as MCLK, with no divider. Only the MSPM0C1106 32 MHz
 * SYSOSC base frequency is accepted currently. On failure the HAL requests
 * the documented 4 MHz SYSOSC fallback and returns false.
 */
bool hal_clock_init(uint32_t requested_hz);

/* The clock frequency selected by hal_clock_init(). */
uint32_t hal_clock_mclk_hz(void);

#endif
