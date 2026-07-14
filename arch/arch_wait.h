#ifndef MSPM_ARCH_WAIT_H
#define MSPM_ARCH_WAIT_H

#include "device.h"

/* Architecture-owned idle primitive; it does not configure a timer or IRQ. */
static inline void arch_wait_for_interrupt(void) {
    __WFI();
}

#endif
