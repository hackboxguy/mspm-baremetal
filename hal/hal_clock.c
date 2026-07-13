#include "hal_clock.h"

#include "device.h"

#define HAL_CLOCK_STATUS_POLLS UINT32_C(4096)

static uint32_t g_mclk_hz = HAL_CLOCK_SAFE_HZ;

static void hal_clock_select_sysosc_div1(void) {
    uint32_t mclk_config;
    uint32_t sysosc_config;

    sysosc_config = SYSCTL->SOCLOCK.SYSOSCCFG;
    sysosc_config &= ~SYSCTL_SYSOSCCFG_DISABLE_MASK;
    SYSCTL->SOCLOCK.SYSOSCCFG = sysosc_config;

    mclk_config = SYSCTL->SOCLOCK.MCLKCFG;
    mclk_config &= ~(SYSCTL_MCLKCFG_USEHSCLK_MASK | SYSCTL_MCLKCFG_USELFCLK_MASK |
                     SYSCTL_MCLKCFG_MDIV_MASK);
    SYSCTL->SOCLOCK.MCLKCFG = mclk_config;
    __DSB();
    __ISB();
}

static bool hal_clock_set_sysosc_frequency(uint32_t frequency) {
    uint32_t sysosc_config;
    uint32_t attempts;

    sysosc_config = SYSCTL->SOCLOCK.SYSOSCCFG;
    sysosc_config &= ~SYSCTL_SYSOSCCFG_FREQ_MASK;
    sysosc_config |= frequency;
    SYSCTL->SOCLOCK.SYSOSCCFG = sysosc_config;
    __DSB();
    __ISB();

    for (attempts = 0U; attempts < HAL_CLOCK_STATUS_POLLS; ++attempts) {
        const uint32_t status = SYSCTL->SOCLOCK.CLKSTATUS;
        const bool sysosc_ready =
            (status & SYSCTL_CLKSTATUS_SYSOSCFREQ_MASK) == frequency;
        const bool sysosc_selected =
            (status &
             (SYSCTL_CLKSTATUS_HSCLKMUX_MASK | SYSCTL_CLKSTATUS_CURMCLKSEL_MASK)) == 0U;

        if (sysosc_ready && sysosc_selected) {
            return true;
        }
    }

    return false;
}

bool hal_clock_init(uint32_t requested_hz) {
    hal_clock_select_sysosc_div1();

    if ((requested_hz == HAL_CLOCK_MSPM0C1106_MAX_HZ) &&
        hal_clock_set_sysosc_frequency(SYSCTL_SYSOSCCFG_FREQ_SYSOSCBASE)) {
        g_mclk_hz = HAL_CLOCK_MSPM0C1106_MAX_HZ;
        return true;
    }

    /* Safe fallback: SYSOSC base is unavailable or the request was invalid. */
    (void)hal_clock_set_sysosc_frequency(SYSCTL_SYSOSCCFG_FREQ_SYSOSC4M);
    g_mclk_hz = HAL_CLOCK_SAFE_HZ;
    return false;
}

uint32_t hal_clock_mclk_hz(void) {
    return g_mclk_hz;
}
