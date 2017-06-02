// File: tmsp-custom-enum.h
//
// *** Generated file. Do not edit! ***
//
// Description: Enumerations for TMSP.
//
// Copyright 2015 Silicon Laboratories, Inc.                                *80*

#ifndef __TMSP_CUSTOM_ENUM_H__
#define __TMSP_CUSTOM_ENUM_H__

//------------------------------------------------------------------------------
// Custom Message Frame IDs

enum {

// NEST_SPECIFIC

  EMBER_PERMIT_JOINING_COMMAND_IDENTIFIER                              = 0x6911,
  EMBER_SET_PASSTHROUGH_PORT_COMMAND_IDENTIFIER                        = 0x6931,
  EMBER_SET_CLOCK_MONITOR_COMMAND_IDENTIFIER                           = 0x6932,
  CB_PERMIT_JOINING_RETURN_COMMAND_IDENTIFIER                          = 0x630D,
  CB_PERMIT_JOINING_HANDLER_COMMAND_IDENTIFIER                         = 0x630E,

// NEST_GPIO

  EMBER_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER                         = 0x6933,
  EMBER_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER                          = 0x6934,
  EMBER_GPIO_POWER_CONFIG_COMMAND_IDENTIFIER                           = 0x6935,
  EMBER_GPIO_RADIO_POWER_MASK_COMMAND_IDENTIFIER                       = 0x6936,
  CB_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER                            = 0x632A,
  CB_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER                             = 0x632B,
  CB_GPIO_POWER_CONFIG_COMMAND_IDENTIFIER                              = 0x632C,

// EMBER_WAKEUP_STACK

  EMBER_CONFIGURE_WAKEUP_COMMAND_IDENTIFIER                            = 0x6922,
  EMBER_SLEEP_NO_TIMER_COMMAND_IDENTIFIER                              = 0x691E,
  EMBER_LISTEN_FOR_WAKEUP_COMMAND_IDENTIFIER                           = 0x691F,
  EMBER_START_WAKEUP_COMMAND_IDENTIFIER                                = 0x6920,
  EMBER_ABORT_WAKEUP_COMMAND_IDENTIFIER                                = 0x6921,
  EMBER_WAKEUP_STATE_COMMAND_IDENTIFIER                                = 0x6923,
  EMBER_ACTIVE_WAKEUP_COMMAND_IDENTIFIER                               = 0x694E,
  EMBER_SET_WAKEUP_SEQUENCE_NUMBER_COMMAND_IDENTIFIER                  = 0x6960,
  EMBER_SET_LEGACY_FILTER_MAX_THRESHOLD_COMMAND_IDENTIFIER             = 0x6985,
  EMBER_SET_LEGACY_FILTER_MIN_THRESHOLD_COMMAND_IDENTIFIER             = 0x6986,
  CB_WAKEUP_COMMAND_IDENTIFIER                                         = 0x6319,
  CB_WAKEUP_STATE_COMMAND_IDENTIFIER                                   = 0x631A,
  CB_LISTEN_FOR_WAKEUP_COMMAND_IDENTIFIER                              = 0x631B,
  CB_START_WAKEUP_COMMAND_IDENTIFIER                                   = 0x631C,
  CB_ABORT_WAKEUP_COMMAND_IDENTIFIER                                   = 0x631D,
  CB_SLEEP_NO_TIMER_COMMAND_IDENTIFIER                                 = 0x632E,
  CB_ACTIVE_WAKEUP_COMMAND_IDENTIFIER                                  = 0x634B,
  CB_SET_WAKEUP_SEQUENCE_NUMBER_COMMAND_IDENTIFIER                     = 0x6357,

  EMBER_WRITE_NEST_MAC_ADDR_COMMAND_IDENTIFIER                         = 0x6990,
  EMBER_NEST_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER               = 0x6991,
  EMBER_NEST_GET_STACK_VERSIONS_COMMAND_IDENTIFIER                     = 0x6992,
  EMBER_NEST_SET_COEX_PARAMETERS_IDENTIFIER                            = 0x6993,
  EMBER_NEST_GET_COEX_STATS_IDENTIFIER                                 = 0x6994,
  CB_WRITE_NEST_MAC_ADDR_COMMAND_IDENTIFIER                            = 0x6390,
  CB_NEST_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER                  = 0x6391,
  CB_NEST_GET_STACK_VERSIONS_COMMAND_IDENTIFIER                        = 0x6392,
  CB_NEST_SET_COEX_PARAMETERS_IDENTIFIER                               = 0x6393,
  CB_NEST_GET_COEX_STATS_IDENTIFIER                                    = 0x6394,

  CB_SET_LEGACY_FILTER_MAX_THRESHOLD_COMMAND_IDENTIFIER                = 0x637d,
  CB_SET_LEGACY_FILTER_MIN_THRESHOLD_COMMAND_IDENTIFIER                = 0x637e,
};

#endif // __TMSP_CUSTOM_ENUM_H__
