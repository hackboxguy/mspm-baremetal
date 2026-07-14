#ifndef MSPM_HAL_I2C_TARGET_ENGINE_H
#define MSPM_HAL_I2C_TARGET_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "lib_regmap.h"

typedef enum {
    HAL_I2C_TARGET_ENGINE_IDLE = 0,
    HAL_I2C_TARGET_ENGINE_RECEIVE,
    HAL_I2C_TARGET_ENGINE_TRANSMIT,
} hal_i2c_target_engine_direction_t;

/*
 * Register-free EEPROM-style transaction engine. The I2C ISR owns every call
 * except the lib_regmap queue/snapshot producer-consumer interactions already
 * described by lib_regmap's API.
 */
typedef struct {
    lib_regmap_t *regmap;
    uint16_t pending_address;
    uint8_t address_byte_count;
    hal_i2c_target_engine_direction_t direction;
} hal_i2c_target_engine_t;

bool hal_i2c_target_engine_init(hal_i2c_target_engine_t *engine, lib_regmap_t *regmap);
void hal_i2c_target_engine_begin_receive(hal_i2c_target_engine_t *engine);
void hal_i2c_target_engine_begin_transmit(hal_i2c_target_engine_t *engine);
bool hal_i2c_target_engine_receive(hal_i2c_target_engine_t *engine, uint8_t value);
uint8_t hal_i2c_target_engine_transmit(hal_i2c_target_engine_t *engine);
void hal_i2c_target_engine_end(hal_i2c_target_engine_t *engine);
void hal_i2c_target_engine_abort(hal_i2c_target_engine_t *engine);
hal_i2c_target_engine_direction_t
hal_i2c_target_engine_direction(const hal_i2c_target_engine_t *engine);

#endif
