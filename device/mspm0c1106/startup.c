#include <stdint.h>

#include "device.h"
#include "lib_crash.h"

typedef void (*isr_handler_t)(void);

extern uint8_t __stack_top;
extern uint8_t __sram_start;
extern uint8_t __data_load_start;
extern uint8_t __data_start;
extern uint8_t __data_end;
extern uint8_t __bss_start;
extern uint8_t __bss_end;

extern int main(void);

lib_crash_record_t g_crash_record __attribute__((section(".noinit"), used, aligned(4)));

void Default_Handler(void) __attribute__((naked));
void Reset_Handler(void);
void NMI_Handler(void) __attribute__((naked, noreturn));
void HardFault_Handler(void) __attribute__((naked, noreturn));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

#define VECTOR(handler) ((uintptr_t)(handler))
#define WEAK_DEFAULT_HANDLER(handler)                                                  \
    void handler(void) __attribute__((weak, alias("Default_Handler")));

WEAK_DEFAULT_HANDLER(SYSCTL_IRQHandler)
WEAK_DEFAULT_HANDLER(DEBUGSS_IRQHandler)
WEAK_DEFAULT_HANDLER(TIMG8_IRQHandler)
WEAK_DEFAULT_HANDLER(UART1_IRQHandler)
WEAK_DEFAULT_HANDLER(ADC0_IRQHandler)
WEAK_DEFAULT_HANDLER(COMP0_IRQHandler)
WEAK_DEFAULT_HANDLER(UART2_IRQHandler)
WEAK_DEFAULT_HANDLER(SPI0_IRQHandler)
WEAK_DEFAULT_HANDLER(UART0_IRQHandler)
WEAK_DEFAULT_HANDLER(TIMG14_IRQHandler)
WEAK_DEFAULT_HANDLER(TIMG2_IRQHandler)
WEAK_DEFAULT_HANDLER(TIMA0_IRQHandler)
WEAK_DEFAULT_HANDLER(TIMG1_IRQHandler)
WEAK_DEFAULT_HANDLER(GPIOA_IRQHandler)
WEAK_DEFAULT_HANDLER(GPIOB_IRQHandler)
WEAK_DEFAULT_HANDLER(I2C0_IRQHandler)
WEAK_DEFAULT_HANDLER(I2C1_IRQHandler)
WEAK_DEFAULT_HANDLER(FLASHCTL_IRQHandler)
WEAK_DEFAULT_HANDLER(WWDT0_IRQHandler)
/* LFSS_INT_IRQn and RTC_B_INT_IRQn share external vector slot 30. */
WEAK_DEFAULT_HANDLER(LFSS_IRQHandler)
WEAK_DEFAULT_HANDLER(DMA_IRQHandler)

#undef WEAK_DEFAULT_HANDLER

#define EXTERNAL_VECTORS                                                               \
    VECTOR(SYSCTL_IRQHandler), VECTOR(DEBUGSS_IRQHandler), VECTOR(TIMG8_IRQHandler),   \
        VECTOR(UART1_IRQHandler), VECTOR(ADC0_IRQHandler), VECTOR(Default_Handler),    \
        VECTOR(Default_Handler), VECTOR(COMP0_IRQHandler), VECTOR(UART2_IRQHandler),   \
        VECTOR(SPI0_IRQHandler), VECTOR(Default_Handler), VECTOR(Default_Handler),     \
        VECTOR(Default_Handler), VECTOR(Default_Handler), VECTOR(Default_Handler),     \
        VECTOR(UART0_IRQHandler), VECTOR(TIMG14_IRQHandler), VECTOR(TIMG2_IRQHandler), \
        VECTOR(TIMA0_IRQHandler), VECTOR(TIMG1_IRQHandler), VECTOR(Default_Handler),   \
        VECTOR(Default_Handler), VECTOR(GPIOA_IRQHandler), VECTOR(GPIOB_IRQHandler),   \
        VECTOR(I2C0_IRQHandler), VECTOR(I2C1_IRQHandler), VECTOR(Default_Handler),     \
        VECTOR(FLASHCTL_IRQHandler), VECTOR(Default_Handler),                          \
        VECTOR(WWDT0_IRQHandler), VECTOR(LFSS_IRQHandler), VECTOR(DMA_IRQHandler)

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
    EXTERNAL_VECTORS,
};

_Static_assert((sizeof(g_vector_table) / sizeof(g_vector_table[0])) == 48U,
               "MSPM0C1106 has 32 external interrupt vector slots");

__attribute__((noreturn)) void crash_capture_from_values(uint32_t reason,
                                                         uint32_t exception_number,
                                                         uint32_t stacked_pc,
                                                         uint32_t stacked_xpsr) {
    lib_crash_capture(&g_crash_record, (lib_crash_reason_t)reason, exception_number,
                      stacked_pc, stacked_xpsr);
    __DSB();
    NVIC_SystemReset();
    for (;;) {
        __WFI();
    }
}

#define CRASH_STRINGIFY_VALUE_IMPL(value) #value
#define CRASH_STRINGIFY_VALUE(value) CRASH_STRINGIFY_VALUE_IMPL(value)
#define CRASH_HANDLER_ASM(reason_code)                                                 \
    /* Snapshot the frame before the fresh C stack can overwrite it. */                \
    "mrs r0, msp\n"                                                                    \
    "movs r3, #3\n"                                                                    \
    "tst r0, r3\n"                                                                     \
    "bne 1f\n"                                                                         \
    "ldr r3, =__sram_start\n"                                                          \
    "cmp r0, r3\n"                                                                     \
    "blo 1f\n"                                                                         \
    "ldr r3, =__crash_stack_frame_limit\n"                                             \
    "cmp r0, r3\n"                                                                     \
    "bhi 1f\n"                                                                         \
    "ldr r2, [r0, #24]\n"                                                              \
    "ldr r3, [r0, #28]\n"                                                              \
    "b 2f\n"                                                                           \
    "1:\n"                                                                             \
    "movs r2, #0\n"                                                                    \
    "movs r3, #0\n"                                                                    \
    "2:\n"                                                                             \
    "mrs r1, ipsr\n"                                                                   \
    "movs r0, #" CRASH_STRINGIFY_VALUE(                                                \
        reason_code) "\n"                                                              \
                     "movs r4, r3\n"                                                   \
                     "ldr r3, =__stack_top\n"                                          \
                     "msr msp, r3\n"                                                   \
                     "ldr r5, =crash_capture_from_values\n"                            \
                     "movs r3, r4\n"                                                   \
                     "bx r5\n"

void NMI_Handler(void) {
    __asm volatile(CRASH_HANDLER_ASM(LIB_CRASH_REASON_CODE_NMI));
}

void HardFault_Handler(void) {
    __asm volatile(CRASH_HANDLER_ASM(LIB_CRASH_REASON_CODE_HARDFAULT));
}

void Default_Handler(void) {
    __asm volatile(CRASH_HANDLER_ASM(LIB_CRASH_REASON_CODE_UNEXPECTED_EXCEPTION));
}

#undef CRASH_HANDLER_ASM
#undef CRASH_STRINGIFY_VALUE
#undef CRASH_STRINGIFY_VALUE_IMPL

void Reset_Handler(void) {
    uint8_t *source;
    uint8_t *destination;
    const uint32_t reset_cause = SYSCTL->SOCLOCK.RSTCAUSE & SYSCTL_RSTCAUSE_ID_MASK;

    /* RSTCAUSE is read-to-clear, so preserve it before any other C startup. */
    lib_crash_note_boot(&g_crash_record, reset_cause);

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
