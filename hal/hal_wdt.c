#include "hal_wdt.h"

#include "device.h"
#include "hal_power.h"

#define HAL_WDT_RESTART_KEY UINT32_C(0x00A7)

bool hal_wdt_init(void) {
    WWDT0->GPRCM.RSTCTL = WWDT_RSTCTL_KEY_UNLOCK_W | WWDT_RSTCTL_RESETSTKYCLR_CLR |
                          WWDT_RSTCTL_RESETASSERT_ASSERT;
    WWDT0->GPRCM.PWREN = WWDT_PWREN_KEY_UNLOCK_W | WWDT_PWREN_ENABLE_ENABLE;
    hal_power_wait_after_enable();

    /* Halt the counter during an SWD stop; it otherwise runs through WFI. */
    WWDT0->PDBGCTL = WWDT_PDBGCTL_FREE_STOP;
    WWDT0->WWDTCTL1 = WWDT_WWDTCTL1_KEY_UNLOCK_W | WWDT_WWDTCTL1_WINSEL_WIN0;
    WWDT0->WWDTCTL0 = WWDT_WWDTCTL0_KEY_UNLOCK_W | WWDT_WWDTCTL0_PER_EN_15 |
                      WWDT_WWDTCTL0_WINDOW0_SIZE_0 | WWDT_WWDTCTL0_WINDOW1_SIZE_0 |
                      WWDT_WWDTCTL0_STISM_CONT;
    __DSB();

    return (WWDT0->WWDTSTAT & WWDT_WWDTSTAT_RUN_MASK) == WWDT_WWDTSTAT_RUN_ON;
}

void hal_wdt_kick(void) {
    WWDT0->WWDTCNTRST = HAL_WDT_RESTART_KEY;
}
