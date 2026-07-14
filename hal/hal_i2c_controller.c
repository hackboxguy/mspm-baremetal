#include "hal_i2c_controller.h"

#include <stddef.h>

#include "device.h"
#include "hal_power.h"

#define HAL_I2C_CONTROLLER_PINCM_COUNT UINT32_C(251)
#define HAL_I2C_CONTROLLER_TIMING_DENOMINATOR UINT32_C(10)
#define HAL_I2C_CONTROLLER_ERRATA_DELAY_NOPS UINT32_C(3)

#define HAL_I2C_CONTROLLER_EVENT_MASK                                                  \
    (I2C_CPU_INT_IMASK_MNACK_SET | I2C_CPU_INT_IMASK_MARBLOST_SET |                    \
     I2C_CPU_INT_IMASK_TIMEOUTA_SET | I2C_CPU_INT_IMASK_TIMEOUTB_SET)

static I2C_Regs *g_i2c;
static uint32_t g_poll_limit;
static uint32_t g_activation_delay_nops;
static uint32_t g_last_status;
static bool g_initialized;

static bool
hal_i2c_controller_config_is_valid(const hal_i2c_controller_config_t *config) {
    uint32_t denominator;
    uint32_t timer_divider;

    if (config == NULL) {
        return false;
    }

    denominator = HAL_I2C_CONTROLLER_TIMING_DENOMINATOR * config->bus_hz;
    if ((config->instance_base != I2C1_BASE) ||
        (config->scl_pincm_index >= HAL_I2C_CONTROLLER_PINCM_COUNT) ||
        (config->sda_pincm_index >= HAL_I2C_CONTROLLER_PINCM_COUNT) ||
        ((config->scl_pincm_function & ~IOMUX_PINCM_PF_MASK) != 0U) ||
        ((config->sda_pincm_function & ~IOMUX_PINCM_PF_MASK) != 0U) ||
        (config->bus_hz == 0U) || (config->poll_limit == 0U) ||
        (config->scl_low_timeout_count <= UINT8_C(1)) || (denominator == 0U) ||
        ((config->input_clock_hz % denominator) != 0U)) {
        return false;
    }

    timer_divider = config->input_clock_hz / denominator;
    return (timer_divider > 0U) && (timer_divider <= UINT32_C(128));
}

static void hal_i2c_controller_delay(uint32_t count) {
    uint32_t index;

    for (index = 0U; index < count; ++index) {
        __NOP();
    }
}

static void hal_i2c_controller_errata_delay(void) {
    /* I2C_ERR_13 requires three functional-clock cycles before BUSY polling. */
    hal_i2c_controller_delay(HAL_I2C_CONTROLLER_ERRATA_DELAY_NOPS);
}

static hal_i2c_controller_result_t hal_i2c_controller_line_state(void) {
    const uint32_t bus_monitor = g_i2c->MASTER.MBMON;

    if ((bus_monitor & I2C_MBMON_SCL_MASK) == 0U) {
        return HAL_I2C_CONTROLLER_SCL_LOW_TIMEOUT;
    }
    if ((bus_monitor & I2C_MBMON_SDA_MASK) == 0U) {
        return HAL_I2C_CONTROLLER_SDA_LOW;
    }
    return HAL_I2C_CONTROLLER_OK;
}

static hal_i2c_controller_result_t hal_i2c_controller_status_result(uint32_t status) {
    if ((status & I2C_MSR_ARBLST_SET) != 0U) {
        return HAL_I2C_CONTROLLER_ARBITRATION_LOST;
    }
    if ((status & I2C_MSR_ADRACK_SET) != 0U) {
        return HAL_I2C_CONTROLLER_ADDRESS_NACK;
    }
    if ((status & I2C_MSR_DATACK_SET) != 0U) {
        return HAL_I2C_CONTROLLER_DATA_NACK;
    }
    if ((status & I2C_MSR_ERR_SET) != 0U) {
        return HAL_I2C_CONTROLLER_IO_ERROR;
    }
    return HAL_I2C_CONTROLLER_OK;
}

static bool hal_i2c_controller_flush_fifos(void) {
    uint32_t index;

    g_i2c->MASTER.MFIFOCTL = I2C_MFIFOCTL_TXFLUSH_FLUSH | I2C_MFIFOCTL_RXFLUSH_FLUSH;
    for (index = 0U; index < g_poll_limit; ++index) {
        const uint32_t fifo_status = g_i2c->MASTER.MFIFOSR;

        if (((fifo_status & I2C_MFIFOSR_RXFIFOCNT_MASK) == 0U) &&
            ((fifo_status & I2C_MFIFOSR_TXFIFOCNT_MASK) ==
             ((uint32_t)I2C_SYS_FENTRIES << I2C_MFIFOSR_TXFIFOCNT_OFS))) {
            g_i2c->MASTER.MFIFOCTL = 0U;
            return true;
        }
    }

    g_i2c->MASTER.MFIFOCTL = 0U;
    return false;
}

static void hal_i2c_controller_clear_events(void) {
    g_i2c->CPU_INT.ICLR = HAL_I2C_CONTROLLER_EVENT_MASK;
}

static hal_i2c_controller_result_t hal_i2c_controller_wait_for_completion(void) {
    hal_i2c_controller_result_t result = HAL_I2C_CONTROLLER_OK;
    uint32_t index;

    for (index = 0U; index < g_poll_limit; ++index) {
        const uint32_t status = g_i2c->MASTER.MSR;
        const uint32_t events = g_i2c->CPU_INT.RIS;
        const hal_i2c_controller_result_t status_result =
            hal_i2c_controller_status_result(status);

        g_last_status = status;
        if ((events & I2C_CPU_INT_RIS_TIMEOUTA_SET) != 0U) {
            hal_i2c_controller_clear_events();
            result = HAL_I2C_CONTROLLER_SCL_LOW_TIMEOUT;
        } else if ((result == HAL_I2C_CONTROLLER_OK) &&
                   (status_result != HAL_I2C_CONTROLLER_OK)) {
            result = status_result;
        }

        if ((status & I2C_MSR_BUSY_SET) == 0U) {
            return result;
        }
    }

    g_i2c->MASTER.MCTR |= I2C_MCTR_STOP_ENABLE;
    return HAL_I2C_CONTROLLER_TIMEOUT;
}

static hal_i2c_controller_result_t hal_i2c_controller_prepare(void) {
    const hal_i2c_controller_result_t line_result = hal_i2c_controller_line_state();

    if (line_result != HAL_I2C_CONTROLLER_OK) {
        return line_result;
    }
    g_last_status = g_i2c->MASTER.MSR;
    if ((g_last_status & I2C_MSR_BUSY_SET) != 0U) {
        return HAL_I2C_CONTROLLER_BUSY;
    }

    g_i2c->MASTER.MCTR = 0U;
    hal_i2c_controller_clear_events();
    return hal_i2c_controller_flush_fifos() ? HAL_I2C_CONTROLLER_OK
                                            : HAL_I2C_CONTROLLER_TIMEOUT;
}

static void hal_i2c_controller_set_address(uint8_t address, bool read) {
    g_i2c->MASTER.MSA = ((uint32_t)address << I2C_MSA_SADDR_OFS) |
                        (read ? I2C_MSA_DIR_RECEIVE : I2C_MSA_DIR_TRANSMIT);
}

static void hal_i2c_controller_preload_write(const uint8_t *data, uint8_t length) {
    uint8_t index;

    for (index = 0U; index < length; ++index) {
        g_i2c->MASTER.MTXDATA = data[index];
    }
}

static void hal_i2c_controller_start(uint8_t read_length, bool read_after_write) {
    uint32_t control = ((uint32_t)read_length << I2C_MCTR_MBLEN_OFS) |
                       I2C_MCTR_BURSTRUN_ENABLE | I2C_MCTR_START_ENABLE |
                       I2C_MCTR_STOP_ENABLE;

    if (read_after_write) {
        control |= I2C_MCTR_RD_ON_TXEMPTY_ENABLE;
    }
    g_i2c->MASTER.MCTR = control;
    hal_i2c_controller_errata_delay();
}

static bool hal_i2c_controller_read_fifo(uint8_t *data, uint8_t length) {
    uint8_t index;

    if ((g_i2c->MASTER.MFIFOSR & I2C_MFIFOSR_RXFIFOCNT_MASK) < (uint32_t)length) {
        return false;
    }
    for (index = 0U; index < length; ++index) {
        data[index] = (uint8_t)(g_i2c->MASTER.MRXDATA & I2C_MRXDATA_VALUE_MASK);
    }
    return true;
}

bool hal_i2c_controller_init(const hal_i2c_controller_config_t *config) {
    uint32_t timer_divider;

    if (g_initialized || !hal_i2c_controller_config_is_valid(config)) {
        return false;
    }

    timer_divider = config->input_clock_hz /
                    (HAL_I2C_CONTROLLER_TIMING_DENOMINATOR * config->bus_hz);
    g_i2c = (I2C_Regs *)config->instance_base;
    g_poll_limit = config->poll_limit;
    g_activation_delay_nops = config->input_clock_hz / config->bus_hz;
    g_last_status = 0U;

    g_i2c->GPRCM.RSTCTL = I2C_RSTCTL_KEY_UNLOCK_W | I2C_RSTCTL_RESETSTKYCLR_CLR |
                          I2C_RSTCTL_RESETASSERT_ASSERT;
    g_i2c->GPRCM.PWREN = I2C_PWREN_KEY_UNLOCK_W | I2C_PWREN_ENABLE_ENABLE;
    hal_power_wait_after_enable();

    IOMUX->SECCFG.PINCM[config->scl_pincm_index] =
        config->scl_pincm_function | IOMUX_PINCM_PC_CONNECTED |
        IOMUX_PINCM_INENA_ENABLE | IOMUX_PINCM_HIZ1_ENABLE;
    IOMUX->SECCFG.PINCM[config->sda_pincm_index] =
        config->sda_pincm_function | IOMUX_PINCM_PC_CONNECTED |
        IOMUX_PINCM_INENA_ENABLE | IOMUX_PINCM_HIZ1_ENABLE;

    g_i2c->CLKSEL = I2C_CLKSEL_BUSCLK_SEL_ENABLE;
    g_i2c->CLKDIV = I2C_CLKDIV_RATIO_DIV_BY_1;
    g_i2c->MASTER.MCTR = 0U;
    g_i2c->MASTER.MTPR = timer_divider - 1U;
    g_i2c->TIMEOUT_CTL =
        (uint32_t)config->scl_low_timeout_count | I2C_TIMEOUT_CTL_TCNTAEN_ENABLE;
    g_i2c->CPU_INT.IMASK = 0U;
    hal_i2c_controller_clear_events();
    g_i2c->MASTER.MCR = I2C_MCR_CLKSTRETCH_ENABLE;
    g_i2c->MASTER.MCR |= I2C_MCR_ACTIVE_ENABLE;
    /* Let BUSBSY observe an external controller before the first MCTR write. */
    hal_i2c_controller_delay(g_activation_delay_nops);

    g_initialized = true;
    return true;
}

hal_i2c_controller_result_t
hal_i2c_controller_write(uint8_t address, const uint8_t *data, uint8_t length) {
    hal_i2c_controller_result_t result;

    if (!g_initialized) {
        return HAL_I2C_CONTROLLER_NOT_INITIALIZED;
    }
    if ((address > UINT8_C(0x7f)) || (data == NULL) || (length == 0U) ||
        (length > HAL_I2C_CONTROLLER_MAX_TRANSFER)) {
        return HAL_I2C_CONTROLLER_INVALID_ARGUMENT;
    }

    result = hal_i2c_controller_prepare();
    if (result != HAL_I2C_CONTROLLER_OK) {
        return result;
    }

    hal_i2c_controller_preload_write(data, length);
    hal_i2c_controller_set_address(address, false);
    hal_i2c_controller_start(length, false);
    result = hal_i2c_controller_wait_for_completion();
    if (result != HAL_I2C_CONTROLLER_OK) {
        (void)hal_i2c_controller_flush_fifos();
    }
    return result;
}

hal_i2c_controller_result_t hal_i2c_controller_read(uint8_t address, uint8_t *data,
                                                    uint8_t length) {
    hal_i2c_controller_result_t result;

    if (!g_initialized) {
        return HAL_I2C_CONTROLLER_NOT_INITIALIZED;
    }
    if ((address > UINT8_C(0x7f)) || (data == NULL) || (length == 0U) ||
        (length > HAL_I2C_CONTROLLER_MAX_TRANSFER)) {
        return HAL_I2C_CONTROLLER_INVALID_ARGUMENT;
    }

    result = hal_i2c_controller_prepare();
    if (result != HAL_I2C_CONTROLLER_OK) {
        return result;
    }

    hal_i2c_controller_set_address(address, true);
    hal_i2c_controller_start(length, false);
    result = hal_i2c_controller_wait_for_completion();
    if (result != HAL_I2C_CONTROLLER_OK) {
        (void)hal_i2c_controller_flush_fifos();
        return result;
    }
    return hal_i2c_controller_read_fifo(data, length) ? HAL_I2C_CONTROLLER_OK
                                                      : HAL_I2C_CONTROLLER_RX_UNDERFLOW;
}

hal_i2c_controller_result_t hal_i2c_controller_write_read(uint8_t address,
                                                          const uint8_t *write_data,
                                                          uint8_t write_length,
                                                          uint8_t *read_data,
                                                          uint8_t read_length) {
    hal_i2c_controller_result_t result;

    if (!g_initialized) {
        return HAL_I2C_CONTROLLER_NOT_INITIALIZED;
    }
    if ((address > UINT8_C(0x7f)) || (write_data == NULL) || (read_data == NULL) ||
        (write_length == 0U) || (read_length == 0U) ||
        (write_length > HAL_I2C_CONTROLLER_MAX_TRANSFER) ||
        (read_length > HAL_I2C_CONTROLLER_MAX_TRANSFER)) {
        return HAL_I2C_CONTROLLER_INVALID_ARGUMENT;
    }

    result = hal_i2c_controller_prepare();
    if (result != HAL_I2C_CONTROLLER_OK) {
        return result;
    }

    hal_i2c_controller_preload_write(write_data, write_length);
    hal_i2c_controller_set_address(address, true);
    hal_i2c_controller_start(read_length, true);
    result = hal_i2c_controller_wait_for_completion();
    if (result != HAL_I2C_CONTROLLER_OK) {
        (void)hal_i2c_controller_flush_fifos();
        return result;
    }
    return hal_i2c_controller_read_fifo(read_data, read_length)
               ? HAL_I2C_CONTROLLER_OK
               : HAL_I2C_CONTROLLER_RX_UNDERFLOW;
}

uint32_t hal_i2c_controller_last_status(void) {
    return g_initialized ? g_last_status : 0U;
}
