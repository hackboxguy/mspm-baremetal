#include "hal_gpio.h"

#include <stddef.h>

#include "device.h"

#define HAL_GPIO_POWER_STARTUP_CPU_CYCLES UINT32_C(64)
#define HAL_GPIO_PINCM_COUNT UINT32_C(251)
#define HAL_GPIO_FUNCTION_DIGITAL_IO UINT32_C(1)

static GPIO_Regs *hal_gpio_port(uintptr_t port_base) {
    return (GPIO_Regs *)port_base;
}

static void hal_gpio_wait_power_startup(void) {
    uint32_t cycle;

    /*
     * The C-Series TRM requires at least four ULPCLK cycles after PWREN before
     * other peripheral registers are accessed. This path runs after the clock
     * HAL has selected 32 MHz SYSOSC or the 4 MHz fallback; 64 CPU NOPs exceed
     * that requirement in either case without mistaking PWREN readback for a
     * peripheral-ready status.
     */
    for (cycle = 0U; cycle < HAL_GPIO_POWER_STARTUP_CPU_CYCLES; ++cycle) {
        __NOP();
    }
}

void hal_gpio_port_reset_enable(uintptr_t port_base) {
    GPIO_Regs *const port = hal_gpio_port(port_base);

    port->GPRCM.RSTCTL = GPIO_RSTCTL_KEY_UNLOCK_W | GPIO_RSTCTL_RESETSTKYCLR_CLR |
                         GPIO_RSTCTL_RESETASSERT_ASSERT;
    port->GPRCM.PWREN = GPIO_PWREN_KEY_UNLOCK_W | GPIO_PWREN_ENABLE_ENABLE;
    __DSB();
    __ISB();
    hal_gpio_wait_power_startup();
}

bool hal_gpio_output_init(const hal_gpio_output_t *output) {
    GPIO_Regs *port;

    if (output == NULL) {
        return false;
    }

    if ((output->pin_mask == 0U) || (output->pincm_index >= HAL_GPIO_PINCM_COUNT)) {
        return false;
    }

    port = hal_gpio_port(output->port_base);
    IOMUX->SECCFG.PINCM[output->pincm_index] =
        IOMUX_PINCM_PC_CONNECTED | HAL_GPIO_FUNCTION_DIGITAL_IO;
    port->DOUTCLR31_0 = output->pin_mask;
    port->DOESET31_0 = output->pin_mask;
    return true;
}

void hal_gpio_output_set(const hal_gpio_output_t *output, bool high) {
    GPIO_Regs *port;

    if (output == NULL) {
        return;
    }

    port = hal_gpio_port(output->port_base);
    if (high) {
        port->DOUTSET31_0 = output->pin_mask;
    } else {
        port->DOUTCLR31_0 = output->pin_mask;
    }
}

void hal_gpio_output_toggle(const hal_gpio_output_t *output) {
    GPIO_Regs *port;

    if (output == NULL) {
        return;
    }

    port = hal_gpio_port(output->port_base);
    port->DOUTTGL31_0 = output->pin_mask;
}
