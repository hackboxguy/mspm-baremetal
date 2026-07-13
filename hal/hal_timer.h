#ifndef MSPM_HAL_TIMER_H
#define MSPM_HAL_TIMER_H

#include <stdbool.h>
#include <stdint.h>

/* Starts the Phase 1 SysTick-backed 1 ms timebase at lowest NVIC priority. */
bool hal_timer_init_1ms(void);
uint32_t hal_timer_now_ms(void);
bool hal_timer_deadline_reached(uint32_t deadline_ms);
void hal_timer_wait_for_interrupt(void);

#endif
