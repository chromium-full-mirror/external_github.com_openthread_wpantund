/*
 *
 *    Copyright (c) 2016 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 */

#ifndef wpantund_wpantund_configuration_h
#define wpantund_wpantund_configuration_h

#define EMBER_APPLICATION_HAS_COUNTER_HANDLER
#define EMBER_APPLICATION_HAS_INCOMING_ICMP_HANDLER
#define EMBER_APPLICATION_HAS_WAKEUP_HANDLER
#define EMBER_APPLICATION_HAS_NETWORK_STATUS_CHANGED_HANDLER
#define EMBER_APPLICATION_HAS_UDP_HANDLER
#define EMBER_APPLICATION_HAS_UDP_MULTICAST_HANDLER
#define EMBER_APPLICATION_HAS_BUTTON_TICK
#define EMBER_APPLICATION_HAS_GET_VERSIONS
#define EMBER_APPLICATION_HAS_GET_VERSIONS_RETURN
#define EMBER_APPLICATION_HAS_BINARY_COMMAND_INTERPRETER
#define EMBER_APPLICATION_HAS_SIM_NOTIFY_SERIAL_RX
#define EMBER_APPLICATION_HAS_SIM_NOTIFY_HEADER_RX
#define EMBER_APPLICATION_HAS_ASH_STATUS_HANDLER

#define BUILD_FEATURE_RADIO_COEXISTENCE 1
#endif
