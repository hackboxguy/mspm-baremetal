#ifndef MSPM_HAL_I2C_TARGET_H
#define MSPM_HAL_I2C_TARGET_H

#include <stdbool.h>
#include <stdint.h>

#include "lib_regmap.h"

/* Board-owned wiring for one C-Series I2C target instance. */
typedef struct {
    uintptr_t instance_base;
    int32_t interrupt_number;
    uint32_t scl_pincm_index;
    uint32_t scl_pincm_function;
    uint32_t sda_pincm_index;
    uint32_t sda_pincm_function;
    uint8_t own_address;
} hal_i2c_target_config_t;

/*
 * Opens one 7-bit target. The first C1106 backend uses I2C1; its board owns
 * the pin mux, address, and interrupt instance supplied in config.
 */
bool hal_i2c_target_init(const hal_i2c_target_config_t *config, lib_regmap_t *regmap);
uint32_t hal_i2c_target_error_count(void);

#endif
