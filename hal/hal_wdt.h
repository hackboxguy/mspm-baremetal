#ifndef MSPM_HAL_WDT_H
#define MSPM_HAL_WDT_H

#include <stdbool.h>
#include <stdint.h>

#define HAL_WDT_TIMEOUT_MS UINT32_C(1000)

/* Starts WWDT0 once with a 1-second, zero-closed-window watchdog period. */
bool hal_wdt_init(void);

/* Services the watchdog; call from the health-checked main-loop path only. */
void hal_wdt_kick(void);

#endif
