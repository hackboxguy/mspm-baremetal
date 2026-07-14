#include "hal_power.h"

#include <stdint.h>

#include "device.h"

#define HAL_POWER_STARTUP_CPU_CYCLES UINT32_C(64)

void hal_power_wait_after_enable(void) {
    uint32_t cycle;

    /* C-Series requires at least four ULPCLK cycles after GPRCM.PWREN. */
    __DSB();
    __ISB();
    for (cycle = 0U; cycle < HAL_POWER_STARTUP_CPU_CYCLES; ++cycle) {
        __NOP();
    }
}
