#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arch_critical.h"

_Static_assert(sizeof(uint32_t) == 4U, "GCC atomic ABI requires 32-bit words");
_Static_assert(sizeof(unsigned int) == sizeof(uint32_t),
               "GCC atomic ABI word must match uint32_t");

/*
 * ARMv6-M has no load-exclusive/store-exclusive instructions. GCC therefore
 * lowers C11 read-modify-write operations on 32-bit atomics to these ABI
 * helpers. A short PRIMASK critical section is the correct single-core/ISR
 * implementation for the platform's register-map state. The DMBs make every
 * memory-order argument at least sequentially consistent.
 */
bool __atomic_compare_exchange_4(volatile void *object, void *expected,
                                 unsigned int desired, bool weak, int success_order,
                                 int failure_order) {
    volatile unsigned int *const target = object;
    unsigned int *const expected_value = expected;
    arch_critical_state_t critical_state;
    unsigned int observed;
    bool exchanged;

    (void)weak;
    (void)success_order;
    (void)failure_order;
    if ((target == NULL) || (expected_value == NULL)) {
        return false;
    }

    critical_state = arch_critical_enter();
    __DMB();
    observed = *target;
    if (observed == *expected_value) {
        *target = desired;
        exchanged = true;
    } else {
        *expected_value = observed;
        exchanged = false;
    }
    __DMB();
    arch_critical_exit(critical_state);
    return exchanged;
}

unsigned int __atomic_fetch_sub_4(volatile void *object, unsigned int operand,
                                  int memory_order) {
    volatile unsigned int *const target = object;
    arch_critical_state_t critical_state;
    unsigned int previous;

    (void)memory_order;
    if (target == NULL) {
        return 0U;
    }

    critical_state = arch_critical_enter();
    __DMB();
    previous = *target;
    *target = previous - operand;
    __DMB();
    arch_critical_exit(critical_state);
    return previous;
}
