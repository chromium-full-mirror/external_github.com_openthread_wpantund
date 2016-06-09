// File: tmsp-host-nest-specific.c
//
// *** Generated file. Do not edit! ***
//
// run tmsp-update.sh to regenerate
//
// Description: Host functions for sending Thread management commands
// to the NCP.
//
// TMSP Version: 13.0
//
// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#define EMBER_APPLICATION_HAS_BINARY_COMMAND_INTERPRETER

#include PLATFORM_HEADER
#ifdef CONFIGURATION_HEADER
  #include CONFIGURATION_HEADER
#endif
#include "include/ember.h"
#include "include/thread-debug.h"
#include "app/ip-ncp/binary-management.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/tmsp/tmsp-enum.h"
#include "app/tmsp/tmsp-frame-utilities.h"
#include "app/tmsp/tmsp-host-utilities.h"
#include "stack/ip/host/host-mfglib.h"
#include "app/tmsp/tmsp-custom-enum.h"
#include "stack/ip/wakeup-specific.h"
#include "app/tmsp/tmsp-host-nest-specific.h"

//------------------------------------------------------------------------------
// NEST_SPECIFIC
//------------------------------------------------------------------------------

void emberPermitJoining(uint16_t durationSeconds)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_PERMIT_JOINING_COMMAND_IDENTIFIER,
                                                    "v",
                                                    durationSeconds);
  emberCustomHostToNcpMessage(buffer, length);
}

void emberSetPassthroughPort(uint16_t port)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_SET_PASSTHROUGH_PORT_COMMAND_IDENTIFIER,
                                                    "v",
                                                    port);
  emberCustomHostToNcpMessage(buffer, length);
}

void emberSetClockMonitor(uint8_t enable,
                          uint32_t timerId)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_SET_CLOCK_MONITOR_COMMAND_IDENTIFIER,
                                                    "uw",
                                                    enable,
                                                    timerId);
  emberCustomHostToNcpMessage(buffer, length);
}

void emberEnableHostJoinClient(bool enable)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_ENABLE_HOST_JOIN_CLIENT_COMMAND_IDENTIFIER,
                                                    "u",
                                                    enable);
  emberCustomHostToNcpMessage(buffer, length);
}

static void permitJoiningReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberPermitJoiningReturn(status);
}

static void permitJoiningHandlerCallback(void)
{
  bool joiningAllowed;
  joiningAllowed = (bool) emberUnsignedCommandArgument(0);
  emberPermitJoiningHandler(joiningAllowed);
}

//------------------------------------------------------------------------------
// NEST_GPIO
//------------------------------------------------------------------------------
#ifndef IP_MODEM_LIBRARY

void emberGpioCurrentConfig(uint8_t portPin,
                            uint8_t cfg,
                            uint8_t out)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER,
                                                    "uuu",
                                                    portPin,
                                                    cfg,
                                                    out);
  emberCustomHostToNcpMessage(buffer, length);
}

void emberGpioCurrentInput(uint8_t portPin)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER,
                                                    "u",
                                                    portPin);
  emberCustomHostToNcpMessage(buffer, length);
}

void emberGpioPowerConfig(uint8_t portPin,
                          uint8_t puCfg,
                          uint8_t puOut,
                          uint8_t pdCfg,
                          uint8_t pdOut)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_GPIO_POWER_CONFIG_COMMAND_IDENTIFIER,
                                                    "uuuuu",
                                                    portPin,
                                                    puCfg,
                                                    puOut,
                                                    pdCfg,
                                                    pdOut);
  emberCustomHostToNcpMessage(buffer, length);
}

void emberGpioRadioPowerMask(uint32_t mask)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_GPIO_RADIO_POWER_MASK_COMMAND_IDENTIFIER,
                                                    "w",
                                                    mask);
  emberCustomHostToNcpMessage(buffer, length);
}

static void gpioCurrentConfigReturnCallback(void)
{
  uint8_t status;
  status = (uint8_t) emberUnsignedCommandArgument(0);
  emberGpioCurrentConfigReturn(status);
}

static void gpioCurrentInputReturnCallback(void)
{
  uint8_t value;
  value = (uint8_t) emberUnsignedCommandArgument(0);
  emberGpioCurrentInputReturn(value);
}

static void gpioPowerConfigReturnCallback(void)
{
  uint8_t status;
  status = (uint8_t) emberUnsignedCommandArgument(0);
  emberGpioPowerConfigReturn(status);
}
#endif /* IP_MODEM_LIBRARY */

//------------------------------------------------------------------------------
// EMBER_WAKEUP_STACK
//------------------------------------------------------------------------------
#ifdef EMBER_WAKEUP_STACK

void emberConfigureWakeup(uint16_t onUs,
                          uint16_t offMs,
                          uint16_t totalDurationMs,
                          uint16_t phase1DurationMs,
                          uint16_t phase1DurationInitiatorMs,
                          uint8_t phase1CcaMode,
                          uint16_t phase1TxDelayUs,
                          uint8_t phase1JitterExpUs,
                          uint8_t phase2CcaMode,
                          uint16_t phase2TxDelayUs,
                          uint8_t phase2JitterExpUs)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_CONFIGURE_WAKEUP_COMMAND_IDENTIFIER,
                                                    "vvvvvuvuuvu",
                                                    onUs,
                                                    offMs,
                                                    totalDurationMs,
                                                    phase1DurationMs,
                                                    phase1DurationInitiatorMs,
                                                    phase1CcaMode,
                                                    phase1TxDelayUs,
                                                    phase1JitterExpUs,
                                                    phase2CcaMode,
                                                    phase2TxDelayUs,
                                                    phase2JitterExpUs);
  emberCustomHostToNcpMessage(buffer, length);
}

void emberSleepNoTimer(void)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_SLEEP_NO_TIMER_COMMAND_IDENTIFIER,
                                                    "");
  emberCustomHostToNcpMessage(buffer, length);
}

void emberListenForWakeup(EmberSleepType sleep)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_LISTEN_FOR_WAKEUP_COMMAND_IDENTIFIER,
                                                    "u",
                                                    sleep);
  emberCustomHostToNcpMessage(buffer, length);
}

void emberStartWakeup(uint8_t dataByte)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_START_WAKEUP_COMMAND_IDENTIFIER,
                                                    "u",
                                                    dataByte);
  emberCustomHostToNcpMessage(buffer, length);
}

void emberAbortWakeup(void)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_ABORT_WAKEUP_COMMAND_IDENTIFIER,
                                                    "");
  emberCustomHostToNcpMessage(buffer, length);
}

void emberWakeupState(void)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_WAKEUP_STATE_COMMAND_IDENTIFIER,
                                                    "");
  emberCustomHostToNcpMessage(buffer, length);
}

void emberActiveWakeup(bool enableActiveWakeup)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_ACTIVE_WAKEUP_COMMAND_IDENTIFIER,
                                                    "u",
                                                    enableActiveWakeup);
  emberCustomHostToNcpMessage(buffer, length);
}

void emberSetWakeupSequenceNumber(uint16_t sequenceNumber)
{
  uint8_t buffer[EMBER_CUSTOM_COMMAND_BUFFER_LENGTH];
  uint16_t length = emFormatBinaryManagementCommand(buffer,
                                                    sizeof(buffer),
                                                    EMBER_SET_WAKEUP_SEQUENCE_NUMBER_COMMAND_IDENTIFIER,
                                                    "v",
                                                    sequenceNumber);
  emberCustomHostToNcpMessage(buffer, length);
}

static void wakeupHandlerCallback(void)
{
  EmberWakeupReason reason;
  EmberWakeupState state;
  uint16_t remainingMs;
  uint8_t dataByte;
  uint16_t otaSequence;
  reason = (EmberWakeupReason) emberUnsignedCommandArgument(0);
  state = (EmberWakeupState) emberUnsignedCommandArgument(1);
  remainingMs = (uint16_t) emberUnsignedCommandArgument(2);
  dataByte = (uint8_t) emberUnsignedCommandArgument(3);
  otaSequence = (uint16_t) emberUnsignedCommandArgument(4);
  emberWakeupHandler(reason,
                     state,
                     remainingMs,
                     dataByte,
                     otaSequence);
}

static void wakeupStateReturnCallback(void)
{
  uint8_t wakeupState;
  uint16_t wakeupSequenceNumber;
  wakeupState = (uint8_t) emberUnsignedCommandArgument(0);
  wakeupSequenceNumber = (uint16_t) emberUnsignedCommandArgument(1);
  emberWakeupStateReturn(wakeupState,
                         wakeupSequenceNumber);
}

static void listenForWakeupReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberListenForWakeupReturn(status);
}

static void startWakeupReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberStartWakeupReturn(status);
}

static void abortWakeupReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberAbortWakeupReturn(status);
}

static void sleepNoTimerReturnCallback(void)
{
  emberSleepNoTimerReturn();
}

static void activeWakeupReturnCallback(void)
{
  EmberStatus status;
  status = (EmberStatus) emberUnsignedCommandArgument(0);
  emberActiveWakeupReturn(status);
}

static void setWakeupSequenceNumberReturnCallback(void)
{
  emberSetWakeupSequenceNumberReturn();
}
#endif /* EMBER_WAKEUP_STACK */

#ifndef EMBER_APPLICATION_HAS_CUSTOM_NCP_TO_HOST_MESSAGE_HANDLER
extern const EmberCommandEntry customManagementCallbackCommandTable[];

void emberCustomNcpToHostMessageHandler(const uint8_t *message, uint8_t messageLength)
{
  EmberCommandState customCommandState;
  emberRunCommandInterpreter(true,
                             &customCommandState,
                             customManagementCallbackCommandTable,
                             emberCommandErrorHandler,
                             message,
                             messageLength);
}
#endif // EMBER_APPLICATION_HAS_CUSTOM_NCP_TO_HOST_MESSAGE_HANDLER

//------------------------------------------------------------------------------
// NEST_COEX
//------------------------------------------------------------------------------

void nestEmberSetTransmitHookActive(uint8_t active)
{
  emSendBinaryManagementCommand(EMBER_NEST_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER,
                                "u",
                                active);
}

void nestEmberSetTransmitHookActiveCallback(void)
{
  nestEmberSetTransmitHookActiveReturn();
}

//------------------------------------------------------------------------------
// Command Handler Table

const EmberCommandEntry customManagementCallbackCommandTable[] = {
  emberBinaryCommandEntryAction(CB_PERMIT_JOINING_RETURN_COMMAND_IDENTIFIER,
                                permitJoiningReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_PERMIT_JOINING_HANDLER_COMMAND_IDENTIFIER,
                                permitJoiningHandlerCallback,
                                "u",
                                NULL),
#ifndef IP_MODEM_LIBRARY
  emberBinaryCommandEntryAction(CB_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER,
                                gpioCurrentConfigReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER,
                                gpioCurrentInputReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_GPIO_POWER_CONFIG_COMMAND_IDENTIFIER,
                                gpioPowerConfigReturnCallback,
                                "u",
                                NULL),
#endif /* IP_MODEM_LIBRARY */
#ifdef EMBER_WAKEUP_STACK
  emberBinaryCommandEntryAction(CB_WAKEUP_COMMAND_IDENTIFIER,
                                wakeupHandlerCallback,
                                "uuvuv",
                                NULL),
  emberBinaryCommandEntryAction(CB_WAKEUP_STATE_COMMAND_IDENTIFIER,
                                wakeupStateReturnCallback,
                                "uv",
                                NULL),
  emberBinaryCommandEntryAction(CB_LISTEN_FOR_WAKEUP_COMMAND_IDENTIFIER,
                                listenForWakeupReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_START_WAKEUP_COMMAND_IDENTIFIER,
                                startWakeupReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_ABORT_WAKEUP_COMMAND_IDENTIFIER,
                                abortWakeupReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_SLEEP_NO_TIMER_COMMAND_IDENTIFIER,
                                sleepNoTimerReturnCallback,
                                "",
                                NULL),
  emberBinaryCommandEntryAction(CB_ACTIVE_WAKEUP_COMMAND_IDENTIFIER,
                                activeWakeupReturnCallback,
                                "u",
                                NULL),
  emberBinaryCommandEntryAction(CB_SET_WAKEUP_SEQUENCE_NUMBER_COMMAND_IDENTIFIER,
                                setWakeupSequenceNumberReturnCallback,
                                "",
                                NULL),
#endif /* EMBER_WAKEUP_STACK */
  { NULL }    // terminator
};
