#include "hal_i2c_target.h"

#include <stddef.h>

#include "device.h"
#include "hal_i2c_target_engine.h"
#include "hal_power.h"

#define HAL_I2C_TARGET_PINCM_COUNT UINT32_C(251)
#define HAL_I2C_TARGET_PRIORITY UINT32_C(1)
#define HAL_I2C_TARGET_ERRATA_RXDONE_NOPS UINT32_C(2)

#define HAL_I2C_TARGET_INTERRUPT_MASK                                                  \
    (I2C_CPU_INT_IMASK_SRXDONE_SET | I2C_CPU_INT_IMASK_STXDONE_SET |                   \
     I2C_CPU_INT_IMASK_STXEMPTY_SET | I2C_CPU_INT_IMASK_SSTART_SET |                   \
     I2C_CPU_INT_IMASK_SSTOP_SET | I2C_CPU_INT_IMASK_TIMEOUTA_SET |                    \
     I2C_CPU_INT_IMASK_TIMEOUTB_SET | I2C_CPU_INT_IMASK_STX_UNFL_SET |                 \
     I2C_CPU_INT_IMASK_SRX_OVFL_SET | I2C_CPU_INT_IMASK_SARBLOST_SET |                 \
     I2C_CPU_INT_IMASK_INTR_OVFL_SET)

static I2C_Regs *g_i2c;
static hal_i2c_target_engine_t g_engine;
static volatile uint32_t g_error_count;
static bool g_initialized;
static bool g_start_prehandled;

static bool hal_i2c_target_config_is_valid(const hal_i2c_target_config_t *config,
                                           const lib_regmap_t *regmap) {
    return (config != NULL) && (regmap != NULL) &&
           (config->instance_base == I2C1_BASE) &&
           (config->interrupt_number == (int32_t)I2C1_INT_IRQn) &&
           (config->own_address <= UINT8_C(0x7f)) &&
           (config->scl_pincm_index < HAL_I2C_TARGET_PINCM_COUNT) &&
           (config->sda_pincm_index < HAL_I2C_TARGET_PINCM_COUNT) &&
           ((config->scl_pincm_function & ~IOMUX_PINCM_PF_MASK) == 0U) &&
           ((config->sda_pincm_function & ~IOMUX_PINCM_PF_MASK) == 0U);
}

static void hal_i2c_target_handle_start(void) {
    /* SSTART precedes the address/direction bits; reset only transaction state. */
    hal_i2c_target_engine_abort(&g_engine);
}

static void hal_i2c_target_begin_if_start_pending(void) {
    if ((g_i2c->CPU_INT.RIS & I2C_CPU_INT_RIS_SSTART_SET) != 0U) {
        hal_i2c_target_handle_start();
        /* SSTART remains pending behind a higher-priority data event. */
        g_start_prehandled = true;
    }
}

static void hal_i2c_target_receive_byte(void) {
    uint32_t cycle;
    uint8_t value;

    hal_i2c_target_begin_if_start_pending();
    if (hal_i2c_target_engine_direction(&g_engine) != HAL_I2C_TARGET_ENGINE_RECEIVE) {
        hal_i2c_target_engine_begin_receive(&g_engine);
    }

    /* I2C_ERR_08: RX FIFO needs two module clocks after SRXDONE. */
    for (cycle = 0U; cycle < HAL_I2C_TARGET_ERRATA_RXDONE_NOPS; ++cycle) {
        __NOP();
    }
    value = (uint8_t)(g_i2c->SLAVE.SRXDATA & I2C_SRXDATA_VALUE_MASK);
    if (!hal_i2c_target_engine_receive(&g_engine, value)) {
        ++g_error_count;
    }
}

static void hal_i2c_target_transmit_byte(void) {
    if ((g_i2c->SLAVE.SSR & I2C_SSR_TREQ_SET) == 0U) {
        return;
    }

    hal_i2c_target_begin_if_start_pending();
    if (hal_i2c_target_engine_direction(&g_engine) != HAL_I2C_TARGET_ENGINE_TRANSMIT) {
        hal_i2c_target_engine_begin_transmit(&g_engine);
    }

    /* TREQ guarantees that this byte cannot become stale after the final NACK. */
    g_i2c->SLAVE.STXDATA = hal_i2c_target_engine_transmit(&g_engine);
}

static void hal_i2c_target_abort(void) {
    ++g_error_count;
    hal_i2c_target_engine_abort(&g_engine);
    g_start_prehandled = false;
}

bool hal_i2c_target_init(const hal_i2c_target_config_t *config, lib_regmap_t *regmap) {
    if (g_initialized || !hal_i2c_target_config_is_valid(config, regmap) ||
        !hal_i2c_target_engine_init(&g_engine, regmap)) {
        return false;
    }

    g_i2c = (I2C_Regs *)config->instance_base;
    g_error_count = 0U;
    g_start_prehandled = false;

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
    g_i2c->SLAVE.SOAR =
        (uint32_t)config->own_address | I2C_SOAR_OAREN_ENABLE | I2C_SOAR_SMODE_MODE7;
    g_i2c->SLAVE.SOAR2 = I2C_SOAR2_OAR2EN_DISABLE;
    g_i2c->SLAVE.SCTR = I2C_SCTR_ACTIVE_DISABLE | I2C_SCTR_GENCALL_DISABLE |
                        I2C_SCTR_SCLKSTRETCH_ENABLE | I2C_SCTR_TXEMPTY_ON_TREQ_ENABLE |
                        I2C_SCTR_TXWAIT_STALE_TXFIFO_ENABLE |
                        I2C_SCTR_RXFULL_ON_RREQ_DISABLE |
                        I2C_SCTR_EN_DEFHOSTADR_DISABLE | I2C_SCTR_EN_ALRESPADR_DISABLE |
                        I2C_SCTR_EN_DEFDEVADR_DISABLE | I2C_SCTR_SWUEN_DISABLE;
    /* Automatic ACK permits the target state machine to acknowledge each byte. */
    g_i2c->SLAVE.SACKCTL = 0U;
    g_i2c->CPU_INT.IMASK = 0U;
    g_i2c->CPU_INT.ICLR = HAL_I2C_TARGET_INTERRUPT_MASK;
    g_i2c->CPU_INT.IMASK = HAL_I2C_TARGET_INTERRUPT_MASK;

    g_initialized = true;
    NVIC_SetPriority((IRQn_Type)config->interrupt_number, HAL_I2C_TARGET_PRIORITY);
    NVIC_ClearPendingIRQ((IRQn_Type)config->interrupt_number);
    NVIC_EnableIRQ((IRQn_Type)config->interrupt_number);
    /* I2C_ERR_05: ACTIVE is set once here and never toggled in the ISR. */
    g_i2c->SLAVE.SCTR |= I2C_SCTR_ACTIVE_ENABLE;
    return true;
}

uint32_t hal_i2c_target_error_count(void) {
    return g_initialized ? g_error_count : 0U;
}

void I2C1_IRQHandler(void) {
    uint32_t interrupt;

    if (!g_initialized) {
        return;
    }

    for (;;) {
        interrupt = g_i2c->CPU_INT.IIDX & I2C_CPU_INT_IIDX_STAT_MASK;
        if (interrupt == I2C_CPU_INT_IIDX_STAT_NO_INTR) {
            break;
        }

        switch (interrupt) {
        case I2C_CPU_INT_IIDX_STAT_SRXDONEFG:
            hal_i2c_target_receive_byte();
            break;
        case I2C_CPU_INT_IIDX_STAT_STXEMPTY:
            hal_i2c_target_transmit_byte();
            break;
        case I2C_CPU_INT_IIDX_STAT_STXDONEFG:
            hal_i2c_target_transmit_byte();
            break;
        case I2C_CPU_INT_IIDX_STAT_SSTARTFG:
            if (g_start_prehandled) {
                g_start_prehandled = false;
            } else {
                hal_i2c_target_handle_start();
            }
            break;
        case I2C_CPU_INT_IIDX_STAT_SSTOPFG:
            hal_i2c_target_engine_end(&g_engine);
            g_start_prehandled = false;
            break;
        case I2C_CPU_INT_IIDX_STAT_TIMEOUTA:
        case I2C_CPU_INT_IIDX_STAT_TIMEOUTB:
        case I2C_CPU_INT_IIDX_STAT_STX_UNFL:
        case I2C_CPU_INT_IIDX_STAT_SRX_OVFL:
        case I2C_CPU_INT_IIDX_STAT_SARBLOST:
        case I2C_CPU_INT_IIDX_STAT_INTR_OVFL:
            hal_i2c_target_abort();
            break;
        default:
            hal_i2c_target_abort();
            break;
        }
    }
}
