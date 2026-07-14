#ifndef MSPM_ARCH_CRITICAL_H
#define MSPM_ARCH_CRITICAL_H

#include <stdint.h>

#include "device.h"

typedef uint32_t arch_critical_state_t;

/*
 * Save and mask PRIMASK around a short thread/ISR shared-register update.
 * The caller must restore exactly the returned state with arch_critical_exit.
 */
static inline arch_critical_state_t arch_critical_enter(void) {
    const arch_critical_state_t state = __get_PRIMASK();

    __disable_irq();
    return state;
}

static inline void arch_critical_exit(arch_critical_state_t state) {
    __set_PRIMASK(state);
}

#endif
