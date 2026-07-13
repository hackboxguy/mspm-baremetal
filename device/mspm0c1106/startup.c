#include <stdint.h>

#include "device.h"

typedef void (*isr_handler_t)(void);

extern uint8_t __stack_top;
extern uint8_t __data_load_start;
extern uint8_t __data_start;
extern uint8_t __data_end;
extern uint8_t __bss_start;
extern uint8_t __bss_end;

extern int main(void);

void Default_Handler(void);
void Reset_Handler(void);
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

#define VECTOR(handler) ((uintptr_t)(handler))
#define EXTERNAL_DEFAULT_VECTORS                                                       \
    VECTOR(Default_Handler), VECTOR(Default_Handler), VECTOR(Default_Handler),         \
        VECTOR(Default_Handler), VECTOR(Default_Handler), VECTOR(Default_Handler),     \
        VECTOR(Default_Handler), VECTOR(Default_Handler), VECTOR(Default_Handler),     \
        VECTOR(Default_Handler), VECTOR(Default_Handler), VECTOR(Default_Handler),     \
        VECTOR(Default_Handler), VECTOR(Default_Handler), VECTOR(Default_Handler),     \
        VECTOR(Default_Handler), VECTOR(Default_Handler), VECTOR(Default_Handler),     \
        VECTOR(Default_Handler), VECTOR(Default_Handler), VECTOR(Default_Handler),     \
        VECTOR(Default_Handler), VECTOR(Default_Handler), VECTOR(Default_Handler),     \
        VECTOR(Default_Handler), VECTOR(Default_Handler), VECTOR(Default_Handler),     \
        VECTOR(Default_Handler), VECTOR(Default_Handler), VECTOR(Default_Handler),     \
        VECTOR(Default_Handler), VECTOR(Default_Handler)

__attribute__((section(".isr_vector"), used, aligned(256)))
const uintptr_t g_vector_table[] = {
    VECTOR(&__stack_top),
    VECTOR(Reset_Handler),
    VECTOR(NMI_Handler),
    VECTOR(HardFault_Handler),
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    VECTOR(SVC_Handler),
    0U,
    0U,
    VECTOR(PendSV_Handler),
    VECTOR(SysTick_Handler),
    EXTERNAL_DEFAULT_VECTORS,
};

_Static_assert((sizeof(g_vector_table) / sizeof(g_vector_table[0])) == 48U,
               "MSPM0C1106 has 32 external interrupt vector slots");

void Default_Handler(void) {
    for (;;) {
        __WFI();
    }
}

void Reset_Handler(void) {
    uint8_t *source;
    uint8_t *destination;

    source = &__data_load_start;
    destination = &__data_start;
    while (destination < &__data_end) {
        *destination = *source;
        ++destination;
        ++source;
    }

    destination = &__bss_start;
    while (destination < &__bss_end) {
        *destination = 0U;
        ++destination;
    }

    (void)main();
    Default_Handler();
}
