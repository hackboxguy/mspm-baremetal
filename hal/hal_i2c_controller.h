#ifndef MSPM_HAL_I2C_CONTROLLER_H
#define MSPM_HAL_I2C_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

/*
 * The initial polled backend preloads the C1106 four-entry controller FIFO.
 * Larger transfers will move to an interrupt or DMA-backed implementation.
 */
#define HAL_I2C_CONTROLLER_MAX_TRANSFER UINT8_C(4)

typedef enum {
    HAL_I2C_CONTROLLER_OK,
    HAL_I2C_CONTROLLER_INVALID_ARGUMENT,
    HAL_I2C_CONTROLLER_NOT_INITIALIZED,
    HAL_I2C_CONTROLLER_BUSY,
    HAL_I2C_CONTROLLER_ADDRESS_NACK,
    HAL_I2C_CONTROLLER_DATA_NACK,
    HAL_I2C_CONTROLLER_ARBITRATION_LOST,
    HAL_I2C_CONTROLLER_SCL_LOW_TIMEOUT,
    HAL_I2C_CONTROLLER_SDA_LOW,
    HAL_I2C_CONTROLLER_TIMEOUT,
    HAL_I2C_CONTROLLER_RX_UNDERFLOW,
    HAL_I2C_CONTROLLER_IO_ERROR,
} hal_i2c_controller_result_t;

/* Board-owned wiring and timing for one C-Series I2C controller instance. */
typedef struct {
    uintptr_t instance_base;
    uint32_t scl_pincm_index;
    uint32_t scl_pincm_function;
    uint32_t sda_pincm_index;
    uint32_t sda_pincm_function;
    uint32_t input_clock_hz;
    uint32_t bus_hz;
    uint32_t poll_limit;
    uint8_t scl_low_timeout_count;
} hal_i2c_controller_config_t;

/*
 * Opens one 7-bit controller. The C1106 backend is polling-only and emits a
 * STOP for every started transfer. It enables the peripheral SCL-low timeout;
 * a low SDA line before START is reported without GPIO recovery pulses.
 */
bool hal_i2c_controller_init(const hal_i2c_controller_config_t *config);

/* Each operation owns one complete I2C transaction and always requests STOP. */
hal_i2c_controller_result_t
hal_i2c_controller_write(uint8_t address, const uint8_t *data, uint8_t length);
hal_i2c_controller_result_t hal_i2c_controller_read(uint8_t address, uint8_t *data,
                                                    uint8_t length);

/* Writes command bytes, issues a repeated START, then receives read_length bytes. */
hal_i2c_controller_result_t hal_i2c_controller_write_read(uint8_t address,
                                                          const uint8_t *write_data,
                                                          uint8_t write_length,
                                                          uint8_t *read_data,
                                                          uint8_t read_length);

uint32_t hal_i2c_controller_last_status(void);

#endif
