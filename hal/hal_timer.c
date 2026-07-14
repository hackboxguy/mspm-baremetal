#include "hal_timer.h"

#include "device.h"
#include "hal_clock.h"

#define HAL_TIMER_TICK_HZ UINT32_C(1000)
#define HAL_TIMER_LOWEST_PRIORITY UINT32_C(3)

static volatile uint32_t g_now_ms;

void SysTick_Handler(void) {
    ++g_now_ms;
}

bool hal_timer_init_1ms(void) {
    const uint32_t mclk_hz = hal_clock_mclk_hz();
    const uint32_t reload = mclk_hz / HAL_TIMER_TICK_HZ;

    if ((mclk_hz % HAL_TIMER_TICK_HZ) != 0U) {
        return false;
    }

    g_now_ms = 0U;
    if (SysTick_Config(reload) != 0U) {
        return false;
    }

    NVIC_SetPriority(SysTick_IRQn, HAL_TIMER_LOWEST_PRIORITY);
    return true;
}

uint32_t hal_timer_now_ms(void) {
    return g_now_ms;
}

bool hal_timer_deadline_reached(uint32_t deadline_ms) {
    return (uint32_t)(hal_timer_now_ms() - deadline_ms) < UINT32_C(0x80000000);
}
