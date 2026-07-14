#include "board.h"

#include "device.h"
#include "hal_clock.h"
#include "hal_gpio.h"
#include "hal_i2c_target.h"
#include "hal_uart.h"
#include "lib_crash.h"
#include "lib_debug.h"

#define BOARD_UART_TX_QUEUE_CAPACITY UINT32_C(64)

static const hal_gpio_output_t g_red_led = {
    .port_base = GPIOA_BASE,
    .pin_mask = BOARD_LED_RED_MASK,
    .pincm_index = IOMUX_PINCM1,
};

static uint8_t g_uart_tx_storage[BOARD_UART_TX_QUEUE_CAPACITY];
static lib_ringbuf_t g_uart_tx_queue;

extern lib_crash_record_t g_crash_record;

static hal_uart0_tx_config_t g_uart0_tx = {
    .clock_hz = BOARD_MCLK_HZ,
    .baud_rate = BOARD_UART_BACKCHANNEL_BAUD,
    .tx_pincm_index = IOMUX_PINCM17,
    .tx_pincm_function = IOMUX_PINCM17_PF_UART0_TX,
};

static const hal_i2c_target_config_t g_i2c1_target = {
    .instance_base = I2C1_BASE,
    .interrupt_number = (int32_t)I2C1_INT_IRQn,
    .scl_pincm_index = IOMUX_PINCM11,
    .scl_pincm_function = IOMUX_PINCM11_PF_I2C1_SCL,
    .sda_pincm_index = IOMUX_PINCM12,
    .sda_pincm_function = IOMUX_PINCM12_PF_I2C1_SDA,
    .own_address = BOARD_I2C_REGMAP_TARGET_ADDRESS,
};

#if defined(DEBUG_ENABLED)
static uint32_t board_debug_write(const uint8_t *data, uint32_t length) {
    return hal_uart0_tx_write(data, length);
}
#endif

board_init_result_t board_init(void) {
    const bool requested_clock_selected = hal_clock_init(BOARD_MCLK_HZ);

    hal_gpio_port_reset_enable(g_red_led.port_base);

    if (!hal_gpio_output_init(&g_red_led)) {
        return BOARD_INIT_GPIO_FAILURE;
    }

    /* UART0 TX is PB6/PINCM17, so GPIOB must be reset and powered first. */
    hal_gpio_port_reset_enable(GPIOB_BASE);
    if (!lib_ringbuf_init(&g_uart_tx_queue, g_uart_tx_storage,
                          BOARD_UART_TX_QUEUE_CAPACITY)) {
        return BOARD_INIT_UART_FAILURE;
    }

    g_uart0_tx.clock_hz = hal_clock_mclk_hz();
    if (!hal_uart0_tx_init(&g_uart0_tx, &g_uart_tx_queue)) {
        return BOARD_INIT_UART_FAILURE;
    }

#if defined(DEBUG_ENABLED)
    lib_debug_set_writer(board_debug_write);
#endif

    if (!requested_clock_selected) {
        return BOARD_INIT_CLOCK_FALLBACK;
    }

    return BOARD_INIT_OK;
}

void board_led_red_set(bool on) {
    hal_gpio_output_set(&g_red_led, on);
}

void board_led_red_toggle(void) {
    hal_gpio_output_toggle(&g_red_led);
}

uint32_t board_uart_backchannel_dropped_count(void) {
    return hal_uart0_tx_dropped_count();
}

bool board_crash_has_fault(void) {
    return lib_crash_has_fault(&g_crash_record);
}

bool board_i2c1_target_init(lib_regmap_t *regmap) {
    return hal_i2c_target_init(&g_i2c1_target, regmap);
}
