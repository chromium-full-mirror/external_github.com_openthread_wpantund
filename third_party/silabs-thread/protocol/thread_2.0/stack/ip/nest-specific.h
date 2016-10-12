/*
 * File: nest-specific.h
 * Description: Nest Specific SOC code.
 *
 * Copyright 2013 by Silicon Laboratories. All rights reserved.             *80*
 */

/** @brief Mark buffers for Nest */
void emMarkNestBuffers(void);

/** @brief  Enables/disables the Nest Clock Monitor on the indicated timer
 * clock.
 * @note This should be implemented in the application, not a library.
 */
void emberSetClockMonitor(uint8_t enable, uint32_t timerId);

/** @brief Set the current GPIO configuration of the indicated port pin.
 */
void emberGpioCurrentConfig(uint8_t portPin, uint8_t cfg, uint8_t out);
void emberGpioCurrentConfigReturn(uint8_t status);

/** @brief  Read the current GPIO input value on the indicated port pin.
 */
void emberGpioCurrentInput(uint8_t portPin);
void emberGpioCurrentInputReturn(uint8_t value);

/** @brief  Set the power-up and power-down (wake / sleep) GPIO
 * configuration of the indicated port pin.
 */
void emberGpioPowerConfig(uint8_t portPin, uint8_t puCfg, uint8_t puOut,
                                         uint8_t pdCfg, uint8_t pdOut);
void emberGpioPowerConfigReturn(uint8_t status);

/** @brief  Set the GPIO radio power mask of GPIOs that are immune to host
 * GPIO manipulation and are reserved to internal NCP radio operation.
 */
void emberGpioRadioPowerMask(uint32_t mask);

/** @brief Allow unsecured inbound TCP and UDP packets on the specified port to
 * be passed up to the host.  A value of 0 turns it off.
 */
void emberSetPassthroughPort(uint16_t port);

/** @brief Enable or disable the transmit hook
 */
void nestEmberSetTransmitHookActive(uint8_t active);

/** @brief Set coexistence parameters
 */
void nestEmberSetCoexParameters(uint8_t mask, uint8_t reserved);
void nestEmberSetCoexParametersReturn(uint8_t status);

/** @brief Get coexistence statistics
 */
void nestEmberGetCoexStats(void);
void nestEmberGetCoexStatsReturn(uint8_t parameters, uint8_t reserved1,
                                          uint8_t state, uint8_t reserved2);
