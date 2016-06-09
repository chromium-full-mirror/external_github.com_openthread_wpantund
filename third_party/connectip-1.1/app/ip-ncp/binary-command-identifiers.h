// File: binary-command-identifiers.h
//
// Description: This file constructs either an enum of command identifiers,
// or a BinaryCommandIdentifierStringMap array, which maps a command identifier
// to a string.
//
// It also describes expected Host and NCP communication that occurs when each
// command is heard. Some unused commands are not described. Callbacks are also
// not described.
//
// How to use this file:
//
// If you want an enum of command identifiers, just #include this file.
// If you want emBinaryCommandIdentifierStringMap, an array of type
// BinaryCommandIdentifierStringMap:
//
// #define MAKE_BINARY_COMMAND_IDENTIFIER_STRINGS
// #include "app/ip-ncp/binary-command-identifiers.h"
//
// Copyright 2014 by Silicon Laboratories. All rights reserved.             *80*

#define MAKE_IDENTIFIER(x, y) (x << 8 | y)

#define QUOTE(x) #x

//
// helpful notes:
//
// Host-to-NCP command identifiers start with 'i', or 0x69
// NCP-to-Host command identifiers start with 'c', or 0x63
//
// .... below signifies one or more of the previous command
//

#ifdef MAKE_BINARY_COMMAND_IDENTIFIER_STRINGS
#  define MAKE_COMMAND_IDENTIFIER(id, code)  \
  {MAKE_IDENTIFIER('i', code), QUOTE(id)},
#  define MAKE_CALLBACK_IDENTIFIER(id, code)  \
  {MAKE_IDENTIFIER('c', code), QUOTE(id)},
typedef struct {
  int16u identifier;
  const char * const string;
} BinaryCommandIdentifierStringMap;

const BinaryCommandIdentifierStringMap emBinaryCommandIdentifierStringMap[] = {
#else
#  define MAKE_COMMAND_IDENTIFIER(id, code) \
  id = MAKE_IDENTIFIER('i', code),
#  define MAKE_CALLBACK_IDENTIFIER(id, code) \
  id = MAKE_IDENTIFIER('c', code),

typedef enum {
#endif

/**
 * @brief EMBER_REBOOT_COMMAND_IDENTIFIER
 *
 *   Reboot the NCP
 *
 *   Arguments: none
 *
 *   Callbacks: CB_RESET_COMMAND_IDENTIFIER
 *              CB_STATE_COMMAND_IDENTIFIER
 *              CB_INIT_HOST_COMMAND_IDENTIFIER
 *              CB_GET_VERSIONS_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_REBOOT_COMMAND_IDENTIFIER, 0x00)

/**
 * @brief EMBER_REBOOT_NETWORK_COMMAND_IDENTIFIER
 *
 *   Reboot the NCP and its network state
 *
 *   Arguments: none
 *
 *   Callbacks: see the callbacks for EMBER_REBOOT_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_REBOOT_NETWORK_COMMAND_IDENTIFIER, 0x01)

/**
 * @brief EMBER_INIT_HOST_COMMAND_IDENTIFIER
 *
 *   Inform the NCP to send the network state and version information
 *
 *   Arguments: none
 *
 *   Callbacks: CB_STATE_COMMAND_IDENTIFIER
 *              CB_INIT_HOST_COMMAND_IDENTIFIER
 *              CB_GET_VERSIONS_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_INIT_HOST_COMMAND_IDENTIFIER, 0x02)

/**
 * @brief EMBER_STATE_COMMAND_IDENTIFIER
 *
 *   This command exists for legacy purposes and is unused
 */
MAKE_COMMAND_IDENTIFIER(EMBER_STATE_COMMAND_IDENTIFIER, 0x03)

/**
 * @brief EMBER_GET_VERSIONS_COMMAND_IDENTIFIER
 *
 *   Get version information from the NCP
 *
 *   Arguments: none
 *
 *   Callbacks: CB_GET_VERSIONS_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GET_VERSIONS_COMMAND_IDENTIFIER, 0x04)

/**
 * @brief EMBER_FORM_COMMAND_IDENTIFIER
 *
 *   Form a network
 *
 *   Arguments: byte array - network id, 16 bytes
 *              byte array - ULA prefix, 8 bytes
 *              int8u      - node type, see the EmberNodeType enum in
 *                           ember-types.h
 *              int8s      - radio TX power
 *              int16u     - options
 *              int32u     - channel mask
 *
 *   Callbacks: CB_SET_RADIO_POWER_COMMAND_IDENTIFIER
 *              CB_STATE_COMMAND_IDENTIFIER
 *              CB_SET_ADDRESS_COMMAND_IDENTIFIER
 *              CB_STATE_COMMAND_IDENTIFIER
 *              CB_FORM_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_FORM_COMMAND_IDENTIFIER, 0x05)

/**
 * @brief EMBER_JOIN_COMMAND_IDENTIFIER
 *
 *   Join a network
 *
 *   Arguments: byte array - network ID, 16 bytes
 *              byte array - extended pan ID, 8 bytes
 *              int16u     - pan ID
 *              int8u      - node type, see the EmberNodeType enum in
 *                           ember-types.h
 *              int8s      - radio TX power
 *              int16u     - options
 *              int32u     - channel mask
 *
 *   Callbacks: CB_SET_RADIO_POWER_COMMAND_IDENTIFIER
 *              CB_STATE_COMMAND_IDENTIFIER
 *              CB_SET_ADDRESS_COMMAND_IDENTIFIER
 *              CB_STATE_COMMAND_IDENTIFIER
 *              CB_JOIN_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_JOIN_COMMAND_IDENTIFIER, 0x06)

/**
 * @brief EMBER_RESUME_COMMAND_IDENTIFIER
 *
 *   Resume network state
 *
 *   Arguments: none
 *
 *   Callbacks: CB_STATE_COMMAND_IDENTIFIER
 *              CB_INIT_HOST_COMMAND_IDENTIFIER
 *              CB_GET_VERSIONS_COMMAND_IDENTIFIER
 *              CB_SET_RADIO_POWER_COMMAND_IDENTIFIER
 *              CB_STATE_COMMAND_IDENTIFIER
 *              CB_SET_ADDRESS_COMMAND_IDENTIFIER
 *              CB_STATE_COMMAND_IDENTIFIER
 *              CB_RESUME_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_RESUME_COMMAND_IDENTIFIER, 0x07)

/**
 * @brief EMBER_GET_NEST_TOKEN_COMMAND_IDENTIFIER
 *
 *   Get the 256-byte token
 *
 *   Arguments: none
 *
 *   Callbacks: CB_GET_NEST_TOKEN_COMMAND_IDENTIFIER
 *              CB_GET_NEST_TOKEN_COMMAND_IDENTIFIER
 *              CB_GET_NEST_TOKEN_COMMAND_IDENTIFIER
 *              CB_GET_NEST_TOKEN_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GET_NEST_TOKEN_COMMAND_IDENTIFIER, 0x08)

/**
 * @brief EMBER_SET_NEST_TOKEN_COMMAND_IDENTIFIER
 *
 *   Set the 256-byte token. Multiply set commands must be performed for each
 *   token set because of a 100 byte limitation in each management command
 *
 *   Arguments: int8u      - the index
 *              byte array - the token chunk
 *
 *   Callbacks: none
 */
MAKE_COMMAND_IDENTIFIER(EMBER_SET_NEST_TOKEN_COMMAND_IDENTIFIER, 0x09)

/**
 * @brief EMBER_PANA_TIMEOUT_COMMAND_IDENTIFIER
 *
 *   This command exists for legacy purposes and is unused
 *
 */
MAKE_COMMAND_IDENTIFIER(EMBER_PANA_TIMEOUT_COMMAND_IDENTIFIER, 0x0A)

/**
 * @brief EMBER_PANA_DROP_COMMAND_IDENTIFIER
 *
 *   This command exists for legacy purposes and is unused
 *
 */
MAKE_COMMAND_IDENTIFIER(EMBER_PANA_DROP_COMMAND_IDENTIFIER, 0x0B)

/**
 * @brief EMBER_PANA_COMPLETE_COMMAND_IDENTIFIER
 *
 *   This command exists for legacy purposes and is unused
 *
 */
MAKE_COMMAND_IDENTIFIER(EMBER_PANA_COMPLETE_COMMAND_IDENTIFIER, 0x0C)

/**
 * @brief EMBER_SET_SECURITY_PARAMS_COMMAND_IDENTIFIER
 *
 *   Set the security parameters
 *
 *   Arguments: byte array - network key, 16 bytes
 *              byte array - preshared key, variable size
 *
 *   Callbacks: CB_SET_SECURITY_PARAMS_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_SET_SECURITY_PARAMS_COMMAND_IDENTIFIER, 0x0D)

/**
 * @brief EMBER_SET_NEXT_NETWORK_KEY_COMMAND_IDENTIFIER
 *
 *   Set the next network key
 *
 *   Arguments: byte array - network key, 16 bytes
 *
 *   Callbacks: CB_SET_NEXT_NETWORK_KEY_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_SET_NEXT_NETWORK_KEY_COMMAND_IDENTIFIER, 0x0E)

/**
 * @brief EMBER_SWITCH_NETWORK_KEY_COMMAND_IDENTIFIER
 *
 *   Switch to the next network key
 *
 *   Arguments: none
 *
 *   Callbacks: CB_SWITCH_NETWORK_KEY_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_SWITCH_NETWORK_KEY_COMMAND_IDENTIFIER, 0x0F)

/**
 * @brief EMBER_GET_NETWORK_KEY_INFO_COMMAND_IDENTIFIER
 *
 *   Get the network key from the NCP
 *
 *   Arguments: int8u - get the key in use (1) or the other key (0)
 *
 *   Callbacks: CB_SET_SECURITY_PARAMS_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GET_NETWORK_KEY_INFO_COMMAND_IDENTIFIER, 0x10)

/**
 * @brief EMBER_PERMIT_JOIN_COMMAND_IDENTIFIER
 *
 *   Permit other nodes to join to this one
 *
 *   Arguments: int16u  - duration in seconds
 *              boolean - network wide
 *
 *   Callbacks: CB_PERMIT_JOIN_RETURN_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_PERMIT_JOIN_COMMAND_IDENTIFIER, 0x11)

/**
 * @brief EMBER_USE_ECC_JOIN_COMMAND_IDENTIFIER
 *
 *   Tell the NCP to use ECC or PSK while joining
 *
 *   Arguments: int8u - use ECC (1) or PSK (0)
 *
 *   Callbacks: none
 */
MAKE_COMMAND_IDENTIFIER(EMBER_USE_ECC_JOIN_COMMAND_IDENTIFIER, 0x12)

/**
 * @brief EMBER_ENERGY_SCAN_COMMAND_IDENTIFIER
 *
 *   Scan channels and report their RSSIs
 *
 *   Arguments: int32u - channel mask
 *              int8u  - duration in frames, the scan lasts 2 << (frames + 1)
 *
 *   Callbacks: CB_ENERGY_SCAN_COMMAND_IDENTIFIER
 *              ....
 */
MAKE_COMMAND_IDENTIFIER(EMBER_ENERGY_SCAN_COMMAND_IDENTIFIER, 0x13)

/**
 * @brief EMBER_ACTIVE_SCAN_COMMAND_IDENTIFIER
 *
 *   Scan channels for beacons and report them
 *
 *   Arguments: int32u - channel mask
 *              int8u  - duration in frames, the scan lasts 2 << (frames + 1)
 *
 *   Callbacks: CB_SCAN_RETURN_COMMAND_IDENTIFIER
 *              ....
 */
MAKE_COMMAND_IDENTIFIER(EMBER_ACTIVE_SCAN_COMMAND_IDENTIFIER, 0x14)

/**
 * @brief EMBER_STOP_SCAN_COMMAND_IDENTIFIER
 *
 *   Stop the energy or active scan
 *
 *   Arguments: none
 *
 *   Callbacks: none
 */
MAKE_COMMAND_IDENTIFIER(EMBER_STOP_SCAN_COMMAND_IDENTIFIER, 0x15)

/**
 * @brief EMBER_SET_RIP_ID_COMMAND_IDENTIFIER
 *
 *   This command exists for legacy purposes and is unused
 */
MAKE_COMMAND_IDENTIFIER(EMBER_SET_RIP_ID_COMMAND_IDENTIFIER, 0x16)

/**
 * @brief EMBER_GET_RIP_ENTRY_COMMAND_IDENTIFIER
 *
 *   This command exists for legacy purposes and is unused
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GET_RIP_ENTRY_COMMAND_IDENTIFIER, 0x17)

/**
 * @brief EMBER_GET_MULTICAST_TABLE_COMMAND_IDENTIFIER
 *
 *   This command exists for legacy purposes and is unused
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GET_MULTICAST_TABLE_COMMAND_IDENTIFIER, 0x18)

/**
 * @brief EMBER_GET_COUNTER_COMMAND_IDENTIFIER
 *
 *   Get a counter value from the NCP
 *
 *   Arguments: int8u - type, see the EmberCounterType enum in ember-types.h
 *
 *   Callbacks: CB_COUNTERS_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GET_COUNTER_COMMAND_IDENTIFIER, 0x19)

/**
 * @brief EMBER_CLEAR_COUNTERS_COMMAND_IDENTIFIER
 *
 *   Clear the counters on the NCP
 *
 *   Arguments: none
 *
 *   Callbacks: none
 */
MAKE_COMMAND_IDENTIFIER(EMBER_CLEAR_COUNTERS_COMMAND_IDENTIFIER, 0x1A)

/**
 * @brief EMBER_FORM_PAN_COMMAND_IDENTIFIER
 *
 *   This command exists for legacy purposes and is unused.
 */
MAKE_COMMAND_IDENTIFIER(EMBER_FORM_PAN_COMMAND_IDENTIFIER, 0x1B)

/**
 * @brief EMBER_SET_TX_POWER_MODE_COMMAND_IDENTIFIER
 *
 *   Set the transmit power mode on the NCP
 *
 *   Arguments: int16u - the power mode
 *
 *   Callbacks: CB_SET_TX_POWER_MODE_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_SET_TX_POWER_MODE_COMMAND_IDENTIFIER, 0x1C)

/**
 * @brief EMBER_GET_TX_POWER_MODE_COMMAND_IDENTIFIER
 *
 *   Get the transmit power mode from the NCP
 *
 *   Arguments: none
 *
 *   Callbacks: CB_GET_TX_POWER_MODE_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GET_TX_POWER_MODE_COMMAND_IDENTIFIER, 0x1D)

/**
 * @brief EMBER_SLEEP_NO_TIMER_COMMAND_IDENTIFIER
 *
 *   Put the NCP to sleep
 *
 *   Arguments: none
 *
 *   Callbacks: CB_SLEEP_NO_TIMER_COMMAND_IDENTIFIER
 *              CB_LISTEN_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_SLEEP_NO_TIMER_COMMAND_IDENTIFIER, 0x1E)

/**
 * @brief EMBER_LISTEN_COMMAND_IDENTIFIER
 *
 *   Listen for OTA wakeups
 *
 *   Arguments: boolean - enable sleep
 *
 *   Callbacks: CB_LISTEN_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_LISTEN_COMMAND_IDENTIFIER, 0x1F)

/**
 * @brief EMBER_START_WAKEUP_COMMAND_IDENTIFIER
 *
 *   Start wakeup
 *
 *   Arguments: int8u - data byte
 *
 *   Callbacks: CB_START_WAKEUP_COMMAND_IDENTIFIER
 *              CB_WAKEUP_COMMAND_IDENTIFIER
 *              ....
 */
MAKE_COMMAND_IDENTIFIER(EMBER_START_WAKEUP_COMMAND_IDENTIFIER, 0x20)

/**
 * @brief EMBER_END_WAKEUP_COMMAND_IDENTIFIER
 *
 *   End wakeup
 *
 *   Arguments: none
 *
 *   Callbacks: CB_WAKEUP_COMMAND_IDENTIFIER
 *              CB_END_WAKEUP_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_END_WAKEUP_COMMAND_IDENTIFIER, 0x21)

/**
 * @brief Callbacks for Command: EMBER_CONFIG_WAKEUP_COMMAND_IDENTIFIER
 *
 *   Configure the wakeup
 *
 *   Arguments: int16u - on (Us)
 *              int16u - off (Ms)
 *              int16u - total duration (Ms)
 *              int16u - phase 1 duration (Ms)
 *              int16u - phase 1 duration initiator (Ms)
 *              int8u  - phase 1 CCA mode
 *              int16u - phase 1 TX delay (Us)
 *              int8u  - phase 1 jitter exp (Us)
 *              int8u  - phase 2 CCA mode
 *              int16u - phase 2 TX delay (Us)
 *              int8u  - phase 2 jitter exp (Us)
 *
 *   Callbacks: none
 */
MAKE_COMMAND_IDENTIFIER(EMBER_CONFIG_WAKEUP_COMMAND_IDENTIFIER, 0x22)

/**
 * @brief EMBER_WAKEUP_STATE_COMMAND_IDENTIFIER
 *
 *   Get the wakeup state
 *
 *   Arguments: none
 *
 *   Callbacks: CB_WAKEUP_STATE_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_WAKEUP_STATE_COMMAND_IDENTIFIER, 0x23)

/**
 * @brief EMBER_FF_WAKEUP_COMMAND_IDENTIFIER
 *
 *   Wake up the NCP
 *
 *   Arguments: none
 *
 *   Callbacks: none
 */
MAKE_COMMAND_IDENTIFIER(EMBER_FF_WAKEUP_COMMAND_IDENTIFIER, 0x24)

/**
 * @brief EMBER_GET_CCA_THRESHOLD_COMMAND_IDENTIFIER
 *
 *   Get the CCA threshold
 *
 *   Arguments: none
 *
 *   Callbacks: CB_GET_CCA_THRESHOLD_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GET_CCA_THRESHOLD_COMMAND_IDENTIFIER, 0x25)

/**
 * @brief EMBER_SET_CCA_THRESHOLD_COMMAND_IDENTIFIER
 *
 *   Set the CCA threshold
 *
 *   Arguments: int8s - threshold
 *
 *   Callbacks: CB_SET_CCA_THRESHOLD_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_SET_CCA_THRESHOLD_COMMAND_IDENTIFIER, 0x26)

/**
 * @brief EMBER_GET_RADIO_POWER_COMMAND_IDENTIFIER
 *
 *   Get the radio power
 *
 *   Arguments: none
 *
 *   Callbacks: CB_GET_RADIO_POWER_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GET_RADIO_POWER_COMMAND_IDENTIFIER, 0x27)

/**
 * @brief EMBER_SET_RADIO_POWER_COMMAND_IDENTIFIER
 *
 *   Set the radio power
 *
 *   Arguments: int8s - power
 *
 *   Callbacks: CB_SET_RADIO_POWER_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_SET_RADIO_POWER_COMMAND_IDENTIFIER, 0x28)

/**
 * @brief EMBER_CONFIG_UART_COMMAND_IDENTIFIER
 *
 *   Debug command. Set drop and corruption percentages for the UART
 *
 *   Arguments: int8u - drop percentage (0 to 100)
 *              int8u - corrupt percentage (0 to 100)
 *
 *   Callbacks: CB_CONFIG_UART_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_CONFIG_UART_COMMAND_IDENTIFIER, 0x29)

/**
 * @brief EMBER_ECHO_COMMAND_IDENTIFIER
 *
 *   Echo a sequence of bytes
 *
 *   Arguments: byte array - the string to echo
 *
 *   Callbacks: CB_ECHO_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_ECHO_COMMAND_IDENTIFIER, 0x2A)

/**
 * @brief EMBER_RESET_NCP_ASH_COMMAND_IDENTIFIER
 *
 *   Test command. Reset the NCP's ASH layer
 *
 *   Arguments: none
 *
 *   Callbacks: CB_RESET_NCP_ASH_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_RESET_NCP_ASH_COMMAND_IDENTIFIER, 0x2B)

/**
 * @brief EMBER_RESET_IP_DRIVER_ASH_COMMAND_IDENTIFIER
 *
 *   Test command. Reset the IP driver's ASH layer
 *
 *   Arguments: none
 *
 *   Callbacks: none
 */
MAKE_COMMAND_IDENTIFIER(EMBER_RESET_IP_DRIVER_ASH_COMMAND_IDENTIFIER, 0x2C)

/**
 * @brief EMBER_RESET_NCP_GPIO_COMMAND_IDENTIFIER
 *
 *   Tell the IP driver to reset the NCP via GPIO. This commands only works on
 *   the Raspberry PI architecture
 *
 *   Arguments: none
 *
 *   Callbacks: see the callbacks for EMBER_REBOOT_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_RESET_NCP_GPIO_COMMAND_IDENTIFIER, 0x2D)

/**
 * @brief EMBER_ENABLE_RESET_NCP_GPIO_COMMAND_IDENTIFIER
 *
 *   Enable or disable the EMBER_RESET_NCP_GPIO_COMMAND_IDENTIFIER command
 *
 *   Arguments: boolean - enable/disable
 *
 *   Callbacks: none
 */
MAKE_COMMAND_IDENTIFIER(EMBER_ENABLE_RESET_NCP_GPIO_COMMAND_IDENTIFIER, 0x2E)

/**
 * @brief EMBER_START_UART_STORM_COMMAND_IDENTIFIER
 *
 *   Test command. Start a storm of UART traffic
 *
 *   Arguments: int16u - the storm rate (Ms)
 *
 *   Callbacks: CB_START_UART_STORM_COMMAND_IDENTIFIER
 *              CB_ACTIVE_SCAN_COMMAND_IDENTIFIER
 *              ....
 */
MAKE_COMMAND_IDENTIFIER(EMBER_START_UART_STORM_COMMAND_IDENTIFIER, 0x2F)

/**
 * @brief EMBER_STOP_UART_STORM_COMMAND_IDENTIFIER
 *
 *   Test command. Stop the UART storm
 *
 *   Arguments: none
 *
 *   Callbacks: CB_STOP_UART_STORM_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_STOP_UART_STORM_COMMAND_IDENTIFIER, 0x30)

/**
 * @brief EMBER_SET_PASSTHROUGH_PORT_COMMAND_IDENTIFIER
 *
 *   Set the passthrough port
 *
 *   Arguments: int16u - the port
 *
 *   Callbacks: none
 */
MAKE_COMMAND_IDENTIFIER(EMBER_SET_PASSTHROUGH_PORT_COMMAND_IDENTIFIER, 0x31)

/**
 * @brief EMBER_SET_NEST_CLOCK_MON_COMMAND_IDENTIFIER
 *
 *   Set clock monitoring configuration
 *
 *   Arguments: boolean - enable/disable
 *              int32u  - timer ID
 *
 *   Callbacks: none
 */
MAKE_COMMAND_IDENTIFIER(EMBER_SET_NEST_CLOCK_MON_COMMAND_IDENTIFIER, 0x32)

/**
 * @brief EMBER_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER
 *
 *   Set the GPIO configuration values
 *
 *   Arguments: int8u - port pin
 *              int8u - configuration
 *              int8u - out
 *
 *   Callbacks: CB_GPIO_CURRENT_CONFIG_RETURN_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER, 0x33)

/**
 * @brief EMBER_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER
 *
 *   Reads the current value of the specified GPIO pin
 *
 *   Arguments: int8u - port pin
 *
 *   Callbacks: CB_GPIO_CURRENT_INPUT_RETURN_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER, 0x34)

/**
 * @brief EMBER_GPIO_POWER_CONFIG_COMMAND_IDENTIFIER
 *
 *   Set the power-up and power-down configuration for all gpio pins
 *
 *   Arguments: int8u - port pin
 *              int8u - power-up configuration
 *              int8u - power-up out
 *              int8u - power-down configuration
 *              int8u - power-down out
 *
 *   Callbacks: CB_GPIO_POWER_CONFIG_RETURN_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GPIO_POWER_CONFIG_COMMAND_IDENTIFIER, 0x35)

/**
 * @brief EMBER_GPIO_RADIO_POWER_MASK_COMMAND_IDENTIFIER
 *
 *   Define the mask for GPIO relevant to the radio in the context
 *   of power state.  Each bit in the mask indicates the corresponding GPIO
 *   which should be affected when invoking halStackRadioPowerUpBoard() or
 *   halStackRadioPowerDownBoard()
 *
 *   Arguments: int32u - mask
 *
 *   Callbacks: none
 */
MAKE_COMMAND_IDENTIFIER(EMBER_GPIO_RADIO_POWER_MASK_COMMAND_IDENTIFIER, 0x36)

/**
 * @brief EMBER_FORCE_ASSERT_COMMAND_IDENTIFIER
 *
 *   Test command. Force the NCP to assert
 *
 *   Arguments: none
 *
 *   Callbacks: see the callbacks for EMBER_REBOOT_COMMAND_IDENTIFIER
 *              CB_ASSERT_INFO_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_FORCE_ASSERT_COMMAND_IDENTIFIER, 0x37)

/**
 * @brief EMBER_MFG_START_TEST_COMMAND_IDENTIFIER
 *
 *   Start the MFG test
 *
 *   Arguments: boolean - request RX callback for each packet
 *
 *   Callbacks: CB_MFG_START_TEST_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_MFG_START_TEST_COMMAND_IDENTIFIER, 0x38)

/**
 * @brief EMBER_MFG_END_TEST_COMMAND_IDENTIFIER
 *
 *   End the MFG test
 *
 *   Arguments: none
 *
 *   Callbacks: CB_MFG_END_TEST_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_MFG_END_TEST_COMMAND_IDENTIFIER, 0x39)

/**
 * @brief EMBER_MFG_START_COMMAND_IDENTIFIER
 *
 *   Start an MFG operation
 *
 *   Argument: int8u - type, 0 for TONE and 1 for STREAM
 *
 *   Callbacks: CB_MFG_START_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_MFG_START_COMMAND_IDENTIFIER, 0x3A)

/**
 * @brief EMBER_MFG_STOP_COMMAND_IDENTIFIER
 *
 *   Stop an MFG operation
 *
 *   Argument: int8u - type, 0 for TONE and 1 for STREAM
 *
 *   Callbacks: CB_MFG_STOP_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_MFG_STOP_COMMAND_IDENTIFIER, 0x3B)

/**
 * @brief EMBER_MFG_SEND_PACKET_COMMAND_IDENTIFIER
 *
 *   Send an MFG packet
 *
 *   Arguments: byte array - packet
 *              int16u     - repeat
 *
 *   Callbacks: CB_MFG_SEND_PACKET_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_MFG_SEND_PACKET_COMMAND_IDENTIFIER, 0x3C)

/**
 * @brief EMBER_MFG_SET_COMMAND_IDENTIFIER
 *
 *   Set an MFG value
 *
 *   Arguments: int8u - value type, 0 = channel
 *                                  1 = power
 *                                  2 = power mode
 *                                  3 = syn offset
 *
 *   Callbacks: CB_MFG_SET_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_MFG_SET_COMMAND_IDENTIFIER, 0x3D)

/**
 * @brief EMBER_MFG_GET_COMMAND_IDENTIFIER
 *
 *   Get an MFG value
 *
 *   Arguments: int8u - value type, 0 = channel
 *                                  1 = power
 *                                  2 = power mode
 *                                  3 = syn offset
 *
 *   Callbacks: the callback depends on the value type given for
 *              EMBER_MFG_GET_COMMAND_IDENTIFIER:
 *
 *              0/channel    = CB_MFG_GET_CHANNEL_COMMAND_IDENTIFIER
 *              1/power      = CB_MFG_GET_POWER_COMMAND_IDENTIFIER
 *              2/power mode = CB_MFG_GET_POWER_MODE_COMMAND_IDENTIFIER
 *              3/syn offset = CB_MFG_GET_SYN_OFFSET_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_MFG_GET_COMMAND_IDENTIFIER, 0x3E)

/**
 * @brief EMBER_MFG_RUN_MOD_DAC_COMMAND_IDENTIFIER
 *
 *   Performs radio calibration on specified channel and for specified duration
 *
 *   Arguments: int8u  - channel
 *              int32u - duration (Ms)
 *
 *   Callbacks: none
 */
MAKE_COMMAND_IDENTIFIER(EMBER_MFG_RUN_MOD_DAC_COMMAND_IDENTIFIER, 0x3F)

//
// more host -> ip-modem-app commands (leaving a few IDs for more mfg commands)
//

/**
 * @brief EMBER_SEND_DONE_COMMMAND_IDENTIFIER
 *
 *   Test command. Tell the NCP to issue a CB_SEND_DONE_COMMAND_IDENTIFIER in
 *   a specific amount of milliseconds
 *
 *   Arguments: int32u - timeout (Ms)
 *
 *   Callbacks: CB_SEND_DONE_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_SEND_DONE_COMMAND_IDENTIFIER, 0x4A)

/**
 * @brief EMBER_ACTIVE_WAKEUP_COMMAND_IDENTIFIER
 *
 *   Enable active wakeup feature on the device, which means that this device
 *   will propagate wakeup packets after being awoken by an over-the-air
 *   packet.
 *
 *   Arguments:  int8u - on / off
 *
 *   Callbacks:  CB_ACTIVE_WAKEUP_COMMAND_IDENTIFIER
 */
MAKE_COMMAND_IDENTIFIER(EMBER_ACTIVE_WAKEUP_COMMAND_IDENTIFIER, 0x4B)

// callbacks (ip-modem-app -> host)

/**
 * @brief CB_RESET_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_RESET_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - see the EmberResetCause enum in network-management.h
 */
MAKE_CALLBACK_IDENTIFIER(CB_RESET_COMMAND_IDENTIFIER, 0x00)

/**
 * @brief CB_STATE_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_STATE_COMMAND_IDENTIFIER
 *
 *   Arguments: byte array - network ID, 16 bytes
 *              byte array - ULA prefix, 8 bytes
 *              byte array - extended pan ID, 8 bytes
 *              int16u     - pan ID
 *              int8u      - channel
 *              int8u      - node type, see the EmberNodeType enum in
 *                           ember-types.h
 *              int8s      - power
 *              int8u      - network status
 *              byte array - EUI64, 8 bytes
 */
MAKE_CALLBACK_IDENTIFIER(CB_STATE_COMMAND_IDENTIFIER, 0x01)

/**
 * @brief CB_GET_VERSIONS_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_GET_VERSIONS_COMMAND_IDENTIFIER
 *
 *   Arguments: byte array - version name
 *              int16u     - management version number
 *              int16u     - stack version number
 *              int16u     - build number
 *              int8u      - version number
 *              byte array - build timestamp
 */
MAKE_CALLBACK_IDENTIFIER(CB_GET_VERSIONS_COMMAND_IDENTIFIER, 0x02)

/**
 * @brief CB_RIP_ENTRY_COMMAND_IDENTIFIER
 *
 *   This command exists for legacy purposes and is unused
 */
MAKE_CALLBACK_IDENTIFIER(CB_RIP_ENTRY_COMMAND_IDENTIFIER, 0x03)

/**
 * @brief CB_INIT_HOST_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_INIT_HOST_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_INIT_HOST_COMMAND_IDENTIFIER, 0x04)

/**
 * @brief CB_COUNTERS_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_GET_COUNTER_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u  - type, see the EmberCounterType enum in ember-types.h
 *              int16u - value
 */
MAKE_CALLBACK_IDENTIFIER(CB_COUNTERS_COMMAND_IDENTIFIER, 0x05)

/**
 * @brief CB_SET_SECURITY_PARAMS_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_SET_SECURITY_PARAMS_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_SET_SECURITY_PARAMS_COMMAND_IDENTIFIER, 0x06)

/**
 * @brief CB_SET_NEXT_NETWORK_KEY_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_SET_NEXT_NETWORK_KEY_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_SET_NEXT_NETWORK_KEY_COMMAND_IDENTIFIER, 0x07)

/**
 * @brief CB_SWITCH_NETWORK_KEY_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_SWITCH_NETWORK_KEY_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_SWITCH_NETWORK_KEY_COMMAND_IDENTIFIER, 0x08)

/**
 * @brief CB_GET_NETWORK_KEY_INFO_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_GET_NETWORK_KEY_INFO_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - key sequence
 *              int8u - key state
 */
MAKE_CALLBACK_IDENTIFIER(CB_GET_NETWORK_KEY_INFO_COMMAND_IDENTIFIER, 0x09)

/**
 * @brief CB_FORM_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_FORM_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_FORM_COMMAND_IDENTIFIER, 0x0A)

/**
 * @brief CB_JOIN_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_JOIN_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_JOIN_COMMAND_IDENTIFIER, 0x0B)

/**
 * @brief CB_RESUME_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_RESUME_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_RESUME_COMMAND_IDENTIFIER, 0x0C)

/**
 * @brief CB_PERMIT_JOIN_RETURN_COMMAND_IDENTIFIER
 *
 *   Callback that is generated when permit joining returns on the NCP
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_PERMIT_JOIN_RETURN_COMMAND_IDENTIFIER, 0x0D)

/**
 * @brief CB_PERMIT_JOIN_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_PERMIT_JOIN_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_PERMIT_JOIN_COMMAND_IDENTIFIER, 0x0E)

/**
 * @brief CB_USE_ECC_JOIN_COMMAND_IDENTIFIER
 *
 *   This callback exists for legacy purposes and is unused
 */
MAKE_CALLBACK_IDENTIFIER(CB_USE_ECC_JOIN_COMMAND_IDENTIFIER, 0x0F)

/**
 * @brief CB_ENERGY_SCAN_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_ENERGY_SCAN_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - channel
 *              int8s - RSSI
 */
MAKE_CALLBACK_IDENTIFIER(CB_ENERGY_SCAN_COMMAND_IDENTIFIER, 0x10)

/**
 * @brief CB_ACTIVE_SCAN_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_ACTIVE_SCAN_COMMAND_IDENTIFIER
 *
 *   Arguments: boolean - allowing join
 *              int8u   - router capacity
 *              int8u   - child capacity
 *              int8u   - channel
 *              int8u   - LQI
 *              int8u   - RSSI
 *              int16u  - short ID
 *              byte array - network ID, 16 bytes
 *              int16u  - pan ID
 *              byte array - extended pan ID, 8 bytes
 *              byte array - EUI64, 8 bytes
 *              int8u      - protocol ID
 *              byte array - network fragment identifier, 5 bytes
 */
MAKE_CALLBACK_IDENTIFIER(CB_ACTIVE_SCAN_COMMAND_IDENTIFIER, 0x11)

/**
 * @brief CB_SCAN_RETURN_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_ACTIVE_SCAN_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - scan return status
 */
MAKE_CALLBACK_IDENTIFIER(CB_SCAN_RETURN_COMMAND_IDENTIFIER, 0x12)

/**
 * @brief CB_SET_ADDRESS_COMMAND_IDENTIFIER
 *
 *   Callback that is generated when the host's address changes
 *
 *   Arguments: byte array - IP address, 16 bytes
 */
MAKE_CALLBACK_IDENTIFIER(CB_SET_ADDRESS_COMMAND_IDENTIFIER, 0x13)

/**
 * @brief CB_SET_DRIVER_ADDRESS_COMMAND_IDENTIFIER
 *
 *   Sent to the IP driver to tell it to change its address
 *
 *   Arguments: byte array - IP address, 16 bytes
 */
MAKE_CALLBACK_IDENTIFIER(CB_SET_DRIVER_ADDRESS_COMMAND_IDENTIFIER, 0x14)

/**
 * @brief CB_START_PANA_CLIENT_COMMAND_IDENTIFIER
 *
 *   Tell the host to start security commissioning
 *
 *   Arguments: byte array - parent IP address, 16 bytes
 */
MAKE_CALLBACK_IDENTIFIER(CB_START_PANA_CLIENT_COMMAND_IDENTIFIER, 0x15)

/**
 * @brief CB_SET_TX_POWER_MODE_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_SET_TX_POWER_MODE_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_SET_TX_POWER_MODE_COMMAND_IDENTIFIER, 0x16)

/**
 * @brief CB_GET_TX_POWER_MODE_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_GET_TX_POWER_MODE_COMMAND_IDENTIFIER
 *
 *   Arguments: int16u - TX power mode
 */
MAKE_CALLBACK_IDENTIFIER(CB_GET_TX_POWER_MODE_COMMAND_IDENTIFIER, 0x17)

/**
 * @brief CB_SET_NETWORK_KEYS_COMMAND_IDENTIFIER
 *
 *   Sent to the IP driver to tell it the network keys
 *
 *   Arguments: int8u      - sequence number
 *              byte array - master key, 16 bytes
 *              int8u      - second sequence number
 *              byte array - second key, 16 bytes
 */
MAKE_CALLBACK_IDENTIFIER(CB_SET_NETWORK_KEYS_COMMAND_IDENTIFIER, 0x18)

/**
 * @brief CB_WAKEUP_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_START_WAKEUP_COMMAND_IDENTIFIER and
 *   EMBER_END_WAKEUP_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u  - reason
 *              int8u  - state
 *              int16u - remaining Ms
 *              int8u  - data byte
 *              int16u - OTA sequence number
 */
MAKE_CALLBACK_IDENTIFIER(CB_WAKEUP_COMMAND_IDENTIFIER, 0x19)

/**
 * @brief CB_WAKEUP_STATE_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_WAKEUP_STATE_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - wakeup state
 *              int8u - wakeup sequence number
 */
MAKE_CALLBACK_IDENTIFIER(CB_WAKEUP_STATE_COMMAND_IDENTIFIER, 0x1A)

/**
 * @brief CB_LISTEN_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_LISTEN_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status, 1 = sleeping, 0 = listening
 */
MAKE_CALLBACK_IDENTIFIER(CB_LISTEN_COMMAND_IDENTIFIER, 0x1B)

/**
 * @brief CB_START_WAKEUP_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_START_WAKEUP_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_START_WAKEUP_COMMAND_IDENTIFIER, 0x1C)

/**
 * @brief CB_END_WAKEUP_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_END_WAKEUP_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_END_WAKEUP_COMMAND_IDENTIFIER, 0x1D)

/**
 * @brief CB_MULTICAST_ENTRY_COMMAND_IDENTIFIER
 *
 *   This callback exists for legacy purposes and is unused
 */
MAKE_CALLBACK_IDENTIFIER(CB_MULTICAST_ENTRY_COMMAND_IDENTIFIER, 0x1E)

/**
 * @brief CB_GET_NEST_TOKEN_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_GET_NEST_TOKEN_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u      - token index
 *              byte array - bytes
 */
MAKE_CALLBACK_IDENTIFIER(CB_GET_NEST_TOKEN_COMMAND_IDENTIFIER, 0x1F)

/**
 * @brief CB_PANA_COMPLETE_COMMAND_IDENTIFIER
 *
 *   This callback exists for legacy purposes and is unused
 */
MAKE_CALLBACK_IDENTIFIER(CB_PANA_COMPLETE_COMMAND_IDENTIFIER, 0x20)

/**
 * @brief CB_GET_CCA_THRESHOLD_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_GET_CCA_THRESHOLD_COMMAND_IDENTIFIER
 *
 *   Arguments: int8s - CCA threshold in dBm
 */
MAKE_CALLBACK_IDENTIFIER(CB_GET_CCA_THRESHOLD_COMMAND_IDENTIFIER, 0x21)

/**
 * @brief CB_SET_CCA_THRESHOLD_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_SET_CCA_THRESHOLD_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_SET_CCA_THRESHOLD_COMMAND_IDENTIFIER, 0x22)

/**
 * @brief CB_GET_RADIO_POWER_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_GET_RADIO_POWER_COMMAND_IDENTIFIER
 *
 *   Arguments: int8s - power in dBm
 */
MAKE_CALLBACK_IDENTIFIER(CB_GET_RADIO_POWER_COMMAND_IDENTIFIER, 0x23)

/**
 * @brief CB_SET_RADIO_POWER_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_SET_RADIO_POWER_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_SET_RADIO_POWER_COMMAND_IDENTIFIER, 0x24)

/**
 * @brief CB_CONFIG_UART_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_CONFIG_UART_COMMAND_IDENTIFIER
 *
 *   Arguments: none
 */
MAKE_CALLBACK_IDENTIFIER(CB_CONFIG_UART_COMMAND_IDENTIFIER, 0x25)

/**
 * @brief CB_ECHO_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_ECHO_COMMAND_IDENTIFIER
 *
 *   Arguments: byte array - echoed bytes
 */
MAKE_CALLBACK_IDENTIFIER(CB_ECHO_COMMAND_IDENTIFIER, 0x26)

/**
 * @brief CB_RESET_NCP_ASH_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_RESET_NCP_ASH_COMMAND_IDENTIFIER
 *
 *   Arguments: none
 */
MAKE_CALLBACK_IDENTIFIER(CB_RESET_NCP_ASH_COMMAND_IDENTIFIER, 0x27)

/**
 * @brief CB_START_UART_STORM_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_START_UART_STORM_COMMAND_IDENTIFIER
 *
 *   Arguments: none
 */
MAKE_CALLBACK_IDENTIFIER(CB_START_UART_STORM_COMMAND_IDENTIFIER, 0x28)

/**
 * @brief CB_STOP_UART_STORM_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_STOP_UART_STORM_COMMAND_IDENTIFIER
 *
 *   Argument: none
 */
MAKE_CALLBACK_IDENTIFIER(CB_STOP_UART_STORM_COMMAND_IDENTIFIER, 0x29)

/**
 * @brief CB_GPIO_CURRENT_CONFIG_RETURN_IDENTIFIER
 *
 *   Callback for EMBER_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 *
 *
 */
MAKE_CALLBACK_IDENTIFIER(CB_GPIO_CURRENT_CONFIG_RETURN_IDENTIFIER, 0x2A)

/**
 * @brief CB_GPIO_CURRENT_INPUT_RETURN_IDENTIFIER
 *
 *   Callback for EMBER_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - value
 */
MAKE_CALLBACK_IDENTIFIER(CB_GPIO_CURRENT_INPUT_RETURN_IDENTIFIER, 0x2B)

/**
 * @brief CB_GPIO_POWER_CONFIG_RETURN_IDENTIFIER
 *
 *   Callback for EMBER_GPIO_POWER_CONFIG_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_GPIO_POWER_CONFIG_RETURN_IDENTIFIER, 0x2C)

/**
 * @brief CB_ASSERT_INFO_COMMAND_IDENTIFIER
 *
 *   Callback that is generated when an assert is encountered, or to
 *   the test command EMBER_FORCE_ASSERT_COMMAND_IDENTIFIER
 *
 *   Arguments: byte array - file name
 *              int32u     - line number
 */
MAKE_CALLBACK_IDENTIFIER(CB_ASSERT_INFO_COMMAND_IDENTIFIER, 0x2D)

/**
 * @brief CB_SLEEP_NO_TIMER_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_SLEEP_NO_TIMER_COMMAND_IDENTIFIER
 *
 *   Arguments: none
 */
MAKE_CALLBACK_IDENTIFIER(CB_SLEEP_NO_TIMER_COMMAND_IDENTIFIER, 0x2E)

//
// mfg callbacks
//

/**
 * @brief CB_MFG_START_TEST_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_MFG_START_TEST_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_START_TEST_COMMAND_IDENTIFIER, 0x2F)

/**
 * @brief CB_MFG_RX_COMMAND_IDENTIFIER
 *
 *   Tell the host that an MFG packet was received
 *
 *   Arguments: byte array - packet
 *              int8u      - LQI
 *              int8s      - RSSI
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_RX_COMMAND_IDENTIFIER, 0x30)

/**
 * @brief CB_MFG_END_TEST_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_MFG_END_TEST_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_END_TEST_COMMAND_IDENTIFIER, 0x31)

/**
 * @brief CB_MFG_START_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_MFG_START_COMMAND
 *
 *   Arguments: int8u - MFG activity, 0 = tone, 1 = stream
 *              int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_START_COMMAND_IDENTIFIER, 0x32)

/**
 * @brief CB_MFG_STOP_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_MFG_STOP_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - MFG activity, 0 = tone, 1 = stream
 *              int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_STOP_COMMAND_IDENTIFIER, 0x33)

/**
 * @brief CB_MFG_SEND_PACKET_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_MFG_SEND_PACKET_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_SEND_PACKET_COMMAND_IDENTIFIER, 0x34)

/**
 * @brief CB_MFG_SET_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_MFG_SET_COMMAND_IDENTIFIER
 *
 *   Arguments: int8u - MFG value: 0 = channel,
 *                                 1 = power,
 *                                 2 = power mode
 *                                 3 = syn offset
 *              int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_SET_COMMAND_IDENTIFIER, 0x35)

/**
 * @brief CB_MFG_GET_COMMAND_IDENTIFIER
 *
 *   This callback exists for legacy purposes and is unused
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_GET_COMMAND_IDENTIFIER, 0x36)

/**
 * @brief CB_MFG_RUN_MOD_DAC_COMMAND_IDENTIFIER
 *
 *   This callback exists for legacy purposes and is unused
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_RUN_MOD_DAC_COMMAND_IDENTIFIER, 0x37)

/**
 * @brief CB_MFG_GET_CHANNEL_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_MFG_GET_COMMAND_IDENTIFIER with CHANNEL as its type
 *
 *   Arguments: int8u - channel
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_GET_CHANNEL_COMMAND_IDENTIFIER, 0x38)

/**
 * @brief CB_MFG_GET_POWER_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_MFG_GET_COMMAND_IDENTIFIER with POWER as its type
 *
 *   Arguments: int8s - power in dB
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_GET_POWER_COMMAND_IDENTIFIER, 0x39)

/**
 * @brief CB_MFG_GET_POWER_MODE_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_MFG_GET_COMMAND_IDENTIFIER with POWER_MODE as its type
 *
 *   Arguments: int16u - TX power mode
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_GET_POWER_MODE_COMMAND_IDENTIFIER, 0x3A)

/**
 * @brief CB_MFG_GET_SYN_OFFSET_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_MFG_GET_COMMAND_IDENTIFIER with SYN_OFFSET as its type
 *
 *   Arguments: int8s - syn offset
 */
MAKE_CALLBACK_IDENTIFIER(CB_MFG_GET_SYN_OFFSET_COMMAND_IDENTIFIER, 0x3B)

//
// more callbacks (leaving a few IDs for more mfg commands)
//

/**
 * @brief CB_SEND_DONE_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_SEND_DONE_COMMAND_IDENTIFIER. Is sent after the "done"
 *   timeout has been reached
 *
 *   Arguments: none
 */
MAKE_CALLBACK_IDENTIFIER(CB_SEND_DONE_COMMAND_IDENTIFIER, 0x47)

/**
 * @brief CB_ACTIVE_WAKEUP_COMMAND_IDENTIFIER
 *
 *   Callback for EMBER_ACTIVE_WAKEUP_COMMAND_IDENTIFIER.
 *
 *   Arguments: int8u - status
 */
MAKE_CALLBACK_IDENTIFIER(CB_ACTIVE_WAKEUP_COMMAND_IDENTIFIER, 0x48)

#ifdef MAKE_BINARY_COMMAND_IDENTIFIER_STRINGS
// end the string list
};
#else
  // end the enum
} BinaryCommandIdentifier;
#endif
