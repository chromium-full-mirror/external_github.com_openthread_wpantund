/*
 * File: host-nest-specific.h
 * Description: Nest Specific Host code.
 *
 * Copyright 2013 by Silicon Laboratories. All rights reserved.             *80*
 */

/** @brief A test command to help verify the validity of token bytes.
 * This command is sent to the host from the NCP.
 */
void getNestTokenCallback(void);
void gpioCurrentConfigReturn(void);
void gpioCurrentInputReturn(void);
void gpioPowerConfigReturn(void);

void nestEmberSetTransmitHookActiveCallback(void);
void nestEmberSetTransmitHookActiveReturn(void);

void nestEmberSetCoexParametersCallback(void);
void nestEmberSetCoexParametersReturn(int8u status);

void nestEmberGetCoexStatsCallback(void);
void nestEmberGetCoexStatsReturn(uint8_t parameters, uint8_t reserved1,
    uint8_t currentState, uint8_t reserved2);

void nestEmberSetTransmitHookActiveCallback(void);
void nestEmberSetTransmitHookActiveReturn(void);

void nestEmberSetCoexParametersCallback(void);
void nestEmberSetCoexParametersReturn(int8u status);

void nestEmberGetCoexStatsCallback(void);
void nestEmberGetCoexStatsReturn(uint8_t parameters, uint8_t reserved1,
    uint8_t currentState, uint8_t reserved2);

// The NCP->Host callback commands.  These are called by the NCP via
// the management interface.  They are not for use from the normal CLI.

#ifdef BUILD_FEATURE_FREERTOS

#ifdef BUILD_FEATURE_RADIO_COEXISTENCE
#define NEST_SPECIFIC_COEX_COMMANDS \
  emberBinaryCommand(CB_NEST_SET_COEX_PARAMETERS_IDENTIFIER,              \
                     nestEmberSetCoexParametersCallback,                  \
                     "u",                                                 \
                     NULL),                                               \
  /* <status>                                                             \
   */                                                                     \
  emberBinaryCommand(CB_NEST_GET_COEX_STATS_IDENTIFIER,                   \
                     nestEmberGetCoexStatsCallback,                       \
                     "uuuu",                                              \
                     NULL),                                               \
  /* <parameters>                                                         \
     <reserved>                                                           \
     <current state>                                                      \
     <reserved2>                                                          \
   */                                                                     \
  emberBinaryCommand(CB_NEST_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER, \
                     nestEmberSetTransmitHookActiveCallback,              \
                     "",                                                  \
                     NULL),                                               \

#else // BUILD_FEATURE_RADIO_COEXISTENCE

#define NEST_SPECIFIC_COEX_COMMANDS

#endif // BUILD_FEATURE_RADIO_COEXISTENCE

#define NEST_SPECIFIC_FREERTOS_COMMANDS \
  NEST_SPECIFIC_COEX_COMMANDS

#else
#define NEST_SPECIFIC_FREERTOS_COMMANDS
#endif

#define NEST_SPECIFIC_CALLBACK_COMMANDS                                 \
  emberBinaryCommand(CB_GET_NEST_TOKEN_COMMAND_IDENTIFIER,              \
                     getNestTokenCallback,                              \
                     "ub",                                              \
                     NULL),                                             \
  /* <token index>                                                      \
   * <bytes>                                                            \
   */                                                                   \
  emberBinaryCommand(CB_GPIO_CURRENT_CONFIG_RETURN_IDENTIFIER,          \
                     gpioCurrentConfigReturn,                           \
                     "u",                                               \
                     NULL),                                             \
  /* <status>                                                           \
   */                                                                   \
  emberBinaryCommand(CB_GPIO_CURRENT_INPUT_RETURN_IDENTIFIER,           \
                     gpioCurrentInputReturn,                            \
                     "u",                                               \
                     NULL),                                             \
  /* <value>                                                            \
   */                                                                   \
  emberBinaryCommand(CB_GPIO_POWER_CONFIG_RETURN_IDENTIFIER,            \
                     gpioPowerConfigReturn,                             \
                     "u",                                               \
                     NULL),                                             \
  /* <status>                                                           \
   */                                                                   \
  NEST_SPECIFIC_FREERTOS_COMMANDS
