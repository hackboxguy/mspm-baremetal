#include "hal_i2c_target_engine.h"

#include <stddef.h>

bool hal_i2c_target_engine_init(hal_i2c_target_engine_t *engine, lib_regmap_t *regmap) {
    if ((engine == NULL) || (regmap == NULL)) {
        return false;
    }

    engine->regmap = regmap;
    engine->pending_address = 0U;
    engine->address_byte_count = 0U;
    engine->direction = HAL_I2C_TARGET_ENGINE_IDLE;
    return true;
}

void hal_i2c_target_engine_begin_receive(hal_i2c_target_engine_t *engine) {
    if ((engine == NULL) || (engine->regmap == NULL)) {
        return;
    }

    /* A write address phase ends any preceding read snapshot. */
    lib_regmap_end_read(engine->regmap);
    engine->pending_address = 0U;
    engine->address_byte_count = 0U;
    engine->direction = HAL_I2C_TARGET_ENGINE_RECEIVE;
}

void hal_i2c_target_engine_begin_transmit(hal_i2c_target_engine_t *engine) {
    if ((engine == NULL) || (engine->regmap == NULL)) {
        return;
    }

    /* A repeated START discards a short address phase before the new read. */
    engine->pending_address = 0U;
    engine->address_byte_count = 0U;
    lib_regmap_begin_read(engine->regmap);
    engine->direction = HAL_I2C_TARGET_ENGINE_TRANSMIT;
}

bool hal_i2c_target_engine_receive(hal_i2c_target_engine_t *engine, uint8_t value) {
    if ((engine == NULL) || (engine->regmap == NULL) ||
        (engine->direction != HAL_I2C_TARGET_ENGINE_RECEIVE)) {
        return false;
    }

    if (engine->address_byte_count == 0U) {
        engine->pending_address = (uint16_t)((uint16_t)value << 8U);
        engine->address_byte_count = 1U;
    } else if (engine->address_byte_count == 1U) {
        engine->pending_address |= value;
        engine->address_byte_count = 2U;
        lib_regmap_set_address(engine->regmap, engine->pending_address);
    } else {
        (void)lib_regmap_write_current(engine->regmap, value);
    }

    return true;
}

uint8_t hal_i2c_target_engine_transmit(hal_i2c_target_engine_t *engine) {
    if ((engine == NULL) || (engine->regmap == NULL) ||
        (engine->direction != HAL_I2C_TARGET_ENGINE_TRANSMIT)) {
        return LIB_REGMAP_UNMAPPED_VALUE;
    }

    return lib_regmap_read_current(engine->regmap);
}

void hal_i2c_target_engine_end(hal_i2c_target_engine_t *engine) {
    if ((engine == NULL) || (engine->regmap == NULL)) {
        return;
    }

    lib_regmap_end_read(engine->regmap);
    engine->pending_address = 0U;
    engine->address_byte_count = 0U;
    engine->direction = HAL_I2C_TARGET_ENGINE_IDLE;
}

void hal_i2c_target_engine_abort(hal_i2c_target_engine_t *engine) {
    hal_i2c_target_engine_end(engine);
}

hal_i2c_target_engine_direction_t
hal_i2c_target_engine_direction(const hal_i2c_target_engine_t *engine) {
    return engine == NULL ? HAL_I2C_TARGET_ENGINE_IDLE : engine->direction;
}
