#ifndef MSPM_HAL_POWER_H
#define MSPM_HAL_POWER_H

/* Wait after setting a peripheral GPRCM.PWREN enable bit before register use. */
void hal_power_wait_after_enable(void);

#endif
