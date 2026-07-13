#include "board.h"

#include "device.h"
#include "hal_clock.h"
#include "hal_gpio.h"

static const hal_gpio_output_t g_red_led = {
    .port_base = GPIOA_BASE,
    .pin_mask = BOARD_LED_RED_MASK,
    .pincm_index = IOMUX_PINCM1,
};

bool board_init(void) {
    const bool requested_clock_selected = hal_clock_init(BOARD_MCLK_HZ);

    if (!hal_gpio_port_reset_enable(g_red_led.port_base)) {
        return false;
    }

    if (!hal_gpio_output_init(&g_red_led)) {
        return false;
    }

    return requested_clock_selected;
}

void board_led_red_set(bool on) {
    hal_gpio_output_set(&g_red_led, on);
}

void board_led_red_toggle(void) {
    hal_gpio_output_toggle(&g_red_led);
}
