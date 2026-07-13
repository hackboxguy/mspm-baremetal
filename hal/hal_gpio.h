#ifndef MSPM_HAL_GPIO_H
#define MSPM_HAL_GPIO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uintptr_t port_base;
    uint32_t pin_mask;
    uint32_t pincm_index;
} hal_gpio_output_t;

/* Reset, enable, and wait for the documented GPIO bus-isolation startup time. */
void hal_gpio_port_reset_enable(uintptr_t port_base);

/* Configure a GPIO output low, then enable its output driver. */
bool hal_gpio_output_init(const hal_gpio_output_t *output);
void hal_gpio_output_set(const hal_gpio_output_t *output, bool high);
void hal_gpio_output_toggle(const hal_gpio_output_t *output);

#endif
