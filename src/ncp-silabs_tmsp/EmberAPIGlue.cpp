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

#if HAVE_CONFIG_H
#include <config.h>
#endif


#include "assert-macros.h"

#include <syslog.h>

#include "EmberNCPInstance.h"
#include "EmberNCPControlInterface.h"

#include "EmberPacked.h"

#ifndef USE_SYSLOG_FOR_EMBER_DEBUG_PRINTS
#define USE_SYSLOG_FOR_EMBER_DEBUG_PRINTS 1
#endif

__BEGIN_DECLS
#include PLATFORM_HEADER
#include "EmberAPIGlue.h"
#include "app/ip-ncp/binary-management.h"
#include "app/util/serial/command-interpreter2.h"
#include "stack/ip/ip-address.h"
#include "app/ip-ncp/serial-link.h"
#include "stack/ip/host/host-address-table.h"
#include "hal/hal.h"
#include "hal/plugin/serial/serial.h"
#include "stack/include/thread-debug.h"
#include "stack/ip/host/host-listener-table.h"
#include "stack/core/ember-stack.h"
#include "stack/framework/event-queue.h"
#include "app/ip-ncp/ip-modem-link.h"
#include "stack/framework/buffer-management.h"
#include "app/tmsp/tmsp-host-nest-specific.h"


#define USE_STUB_emMarkBufferPointer
#define USE_STUB_emberSerialReadByte
#define USE_STUB_emberCloseListeners
#define USE_STUB_emberSerialWaitSend
#define USE_STUB_emCoapInitialize
#define USE_STUB_emberGetCtuneReturn
#define USE_STUB_emberSetCtuneReturn
//#define USE_STUB_emberSetLegacyFilterMaxThresholdReturn
//#define USE_STUB_emberSetLegacyFilterMinThresholdReturn
#include "stack/ip/stubs.c"

static EmberCommandState callbackCommandState = {0};
EventQueue emApiAppEventQueue;

EmberCommandEntry emberCommandTable[] = {
  { NULL }
};


const char *decodeFrameId(uint16_t value);
const char *decodeCustomFrameId(uint16_t value);
__END_DECLS



using namespace nl;
using namespace nl::wpantund;

void
nl::wpantund::EmberGlue::init(void)
{
	emInitializeEventQueue(&emApiAppEventQueue);
}

cms_t
nl::wpantund::EmberGlue::get_ms_to_next_ember_event(void)
{
	uint32_t ember_ms = emberMsToNextQueueEvent(&emApiAppEventQueue);

	if (ember_ms <= CMS_DISTANT_FUTURE) {
		return static_cast<cms_t>(ember_ms);
	}

	return CMS_DISTANT_FUTURE;
}

void
nl::wpantund::EmberGlue::process_ember(void)
{
	emberTick();
	emberRunEventQueue(&emApiAppEventQueue);
}

//void *emBufferMalloc(int16u size)
//{
//	return malloc(size);
//}
//
//void emBufferFree(void *pointer)
//{
//	free(pointer);
//}
//

static void
wpantundCommandErrorHandler(EmberCommandStatus status,
                              EmberCommandEntry *command)
{

	switch(status) {
	case EMBER_CMD_SUCCESS:
	case EMBER_CMD_ERR_PORT_PROBLEM:
	case EMBER_CMD_ERR_NO_SUCH_COMMAND:
		syslog(LOG_ERR, "emberCommandErrorHandler: EmberCommandStatus=%d", status);
		break;

	case EMBER_CMD_ERR_WRONG_NUMBER_OF_ARGUMENTS:
	case EMBER_CMD_ERR_ARGUMENT_OUT_OF_RANGE:
	case EMBER_CMD_ERR_ARGUMENT_SYNTAX_ERROR:
	case EMBER_CMD_ERR_STRING_TOO_LONG:
	case EMBER_CMD_ERR_INVALID_ARGUMENT_TYPE:
	default:
		syslog(LOG_CRIT, "emberCommandErrorHandler: EmberCommandStatus=%d", status);
		//gNCPControlInterface->change_ncp_state(nl::wpantund::NCPControlInterface::FAULT);
		break;
	}
}

__BEGIN_DECLS
void
emberCommandErrorHandler(EmberCommandStatus status,
                              EmberCommandEntry *command)
{
	wpantundCommandErrorHandler(status, command);
}

__END_DECLS


int
nl::wpantund::EmberGlue::get_cmd_id(const uint8_t* data, size_t len)
{
	if (len >= 2) {
		return (data[0] << 8) + data[1];
	}
	return -1;
}

int
nl::wpantund::EmberGlue::unwrap_custom_callback(const uint8_t*&data, size_t&len)
{
	int cmdid(EmberGlue::get_cmd_id(data, len));

	switch(cmdid) {
	case EMBER_CUSTOM_HOST_TO_NCP_MESSAGE_COMMAND_IDENTIFIER:
	case CB_CUSTOM_NCP_TO_HOST_MESSAGE_COMMAND_IDENTIFIER:
		if (EmberCommands::UnpackArguments(
			data,
			len,
			EMBER_PACKED_UINT16 "b",
			NULL,
			&len,
			&data
		) < 0) {
			return -1;
		}
		break;
	}

	return 0;
}

std::string
nl::wpantund::EmberGlue::describe_frame(const uint8_t* data, size_t len)
{

	size_t prevlen(len);
	EmberGlue::unwrap_custom_callback(data, len);
	const char* frame_id_cstr = NULL;

	if (len != prevlen) {
		frame_id_cstr = decodeCustomFrameId(EmberGlue::get_cmd_id(data, len));
		if (frame_id_cstr == NULL) {
			frame_id_cstr = "UNKNOWN";
		}

		return "WRAPPED(" + std::string(frame_id_cstr) + ")";
	}

	frame_id_cstr = decodeFrameId(EmberGlue::get_cmd_id(data, len));
	if (frame_id_cstr == NULL) {
		frame_id_cstr = "UNKNOWN";
	}

	return frame_id_cstr;
}

NCPState
nl::wpantund::EmberGlue::ncp_state_convert_from_ember(EmberNetworkStatus association_state)
{
	NCPState ret;

	switch (association_state) {
	case EMBER_NO_NETWORK:
		ret = OFFLINE;
		break;

	case EMBER_SAVED_NETWORK:
		ret = COMMISSIONED;
		break;

	case EMBER_JOINING_NETWORK:
		ret = ASSOCIATING;
		break;

	case EMBER_JOINED_NETWORK_ATTACHED:
		ret = ASSOCIATED;
		break;

	case EMBER_JOINED_NETWORK_ATTACHING:
	case EMBER_JOINED_NETWORK_NO_PARENT:
		ret = ISOLATED;
		break;

	default:
		ret = FAULT;
		break;
	}

	return ret;
}

NodeType
nl::wpantund::EmberGlue::node_type_convert_from_ember(EmberNodeType node_type)
{
	NodeType ret;

	node_type &= ~EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG;

	switch (node_type) {
	case EMBER_LURKER:
		ret = LURKER;
		break;
	case EMBER_ROUTER:
		ret = ROUTER;
		break;
	case EMBER_END_DEVICE:
		ret = END_DEVICE;
		break;
	case EMBER_SLEEPY_END_DEVICE:
		ret = SLEEPY_END_DEVICE;
		break;
	case EMBER_COMMISSIONER:
		ret = COMMISSIONER;
		break;
	default:
		ret = UNKNOWN;
		break;
	}
	return ret;
}

EmberNodeType
nl::wpantund::EmberGlue::node_type_convert_to_ember(NodeType node_type, bool support_legacy)
{
	uint8_t ncp_node_type;

	switch (node_type) {
	case LURKER:
		ncp_node_type = EMBER_LURKER | EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG;
		break;

	case END_DEVICE:
		ncp_node_type = EMBER_END_DEVICE;
		if (support_legacy) {
			ncp_node_type |= EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG;
		}
		break;

	case SLEEPY_END_DEVICE:
		ncp_node_type = EMBER_SLEEPY_END_DEVICE;
		if (support_legacy) {
			ncp_node_type |= EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG;
		}
		break;

	case COMMISSIONER:
		ncp_node_type = EMBER_COMMISSIONER;
		if (support_legacy) {
			ncp_node_type |= EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG;
		}
		break;

	default:
	case ROUTER:
		ncp_node_type = support_legacy
			? EMBER_ROUTER | EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG
			: EMBER_ROUTER;
		break;
	}
	return ncp_node_type;
}

std::string
nl::wpantund::EmberGlue::configure_gateway_flags_to_string(uint8_t flags)
{
	std::string ret;
	int i;
	for (i = 7; i >=0 ; --i) {
		uint8_t mask = (1<<i);
		if (i == 3) {
			ret += ' ';
		}
		if (!(mask & flags)) {
			ret += '-';
			continue;
		}
		switch (mask) {
		case EMBER_BORDER_ROUTER_DEFAULT_ROUTE_FLAG:
			ret += 'd';
			break;
		case EMBER_BORDER_ROUTER_CONFIGURE_FLAG:
			ret += 'C';
			break;
		case EMBER_BORDER_ROUTER_DHCP_FLAG:
			ret += 'D';
			break;
		case EMBER_BORDER_ROUTER_SLAAC_FLAG:
			ret += 'V';
			break;
		case EMBER_BORDER_ROUTER_PREFERRED_FLAG:
			ret += 'P';
			break;
		case EMBER_BORDER_ROUTER_HIGH_PREFERENCE:
			ret += 'H';
			break;
		case EMBER_BORDER_ROUTER_ON_MESH_FLAG:
			ret += 'M';
			break;
		default:
			ret += '0' + i;
			break;
		}
	}
	return ret;
}

WPAN::NetworkInstance
nl::wpantund::EmberGlue::network_instance_from_ember_network_parameters(const EmberNetworkParameters *parameters)
{
	char network_name[17] = {};

	memcpy(network_name, parameters->networkId, sizeof(parameters->networkId));

	return WPAN::NetworkInstance(
		network_name,
		parameters->extendedPanId,
		parameters->panId,
		parameters->channel
	);
}


static const char* counter_cstr[] = { EMBER_COUNTER_STRINGS };

std::string
nl::wpantund::EmberGlue::counter_type_to_string(EmberCounterType type)
{
	std::string ret;
	const char* next;

	if (type > EMBER_COUNTER_TYPE_COUNT) {
		goto bail;
	}

	for(next = counter_cstr[type]; *next != 0; next++) {
		if (*next == ' ' || *next == '-') {
			ret.append(1, '_');
		} else {
			ret.append(1, toupper(*next));
		}
	}

bail:
	if (ret.empty()) {
		char temp_cstr[64];
		snprintf(temp_cstr, sizeof(temp_cstr), "CNTR-%d", static_cast<uint8_t>(type));
		ret = temp_cstr;
	}

	return ret;
}

EmberCounterType
nl::wpantund::EmberGlue::string_to_counter_type(const std::string& key)
{
	EmberCounterType ret = EMBER_COUNTER_TYPE_COUNT;

	if (key.find("Counter-") == 0) {
		ret = static_cast<uint8_t>(strtol(key.c_str() + 8, NULL, 0));
	}

	if (ret >= EMBER_COUNTER_TYPE_COUNT) {
		char temp_cstr[64];
		int i;

		temp_cstr[sizeof(temp_cstr)-1] = 0;
		for (i=0; i < EMBER_COUNTER_TYPE_COUNT; i++) {
			char* next_space = NULL;
			strncpy(temp_cstr, EmberGlue::counter_type_to_string(i).c_str(), sizeof(temp_cstr)-1);
			while((next_space = strchr(temp_cstr,' '))
				|| (next_space = strchr(temp_cstr,'-'))
			) {
				*next_space = '_';
			}
			if (strcaseequal(temp_cstr, key.c_str())) {
				ret = i;
				break;
			}
		}
	}

	return ret;

}


const char*
nl::wpantund::EmberGlue::reset_cause_to_cstr(EmberResetCause cause)
{
	const char* reason_cstr = NULL;
	switch(cause) {
	case EMBER_RESET_FIB:
		reason_cstr = "FIB";
		break;
	case EMBER_RESET_BOOTLOADER:
		reason_cstr = "BOOTLOADER";
		break;
	case EMBER_RESET_EXTERNAL:
		reason_cstr = "EXTERNAL";
		break;
	case EMBER_RESET_POWERON:
		reason_cstr = "POWERON";
		break;
	case EMBER_RESET_WATCHDOG:
		reason_cstr = "WATCHDOG";
		break;
	case EMBER_RESET_SOFTWARE:
		reason_cstr = "SOFTWARE";
		break;
	case EMBER_RESET_CRASH:
		reason_cstr = "CRASH";
		break;
	case EMBER_RESET_FLASH:
		reason_cstr = "FLASH";
		break;
	case EMBER_RESET_FATAL:
		reason_cstr = "FATAL";
		break;
	case EMBER_RESET_FAULT:
		reason_cstr = "FAULT";
		break;

	default:
	case EMBER_RESET_UNKNOWN:
		reason_cstr = "UNKNOWN";
		break;
	}
	return reason_cstr;
}

void
nl::wpantund::EmberGlue::process_message_from_ncp(
	SerialLinkMessageType type,
	const uint8_t *message,
	uint16_t length
) {
	uint8_t type_as_byte(type);

	if (type == UART_LINK_TYPE_MANAGEMENT) {
		uint8_t managementType = message[0];
		if (managementType == MANAGEMENT_COMMAND) {
			message++;
			length--;

			syslog(
				LOG_INFO,
				"[NCP->] Callback: %s length: %d\n",
				EmberGlue::describe_frame(message, length).c_str(),
				(int)length
			);

			emberRunBinaryCommandInterpreter(
				&callbackCommandState,
				managementCallbackCommandTable,
				&wpantundCommandErrorHandler,
				message,
				length
			);

			// Give the controller a peek at the raw data.
			gNCPControlInterface->handle_ncp_frame(
				managementType,
				message,
				length
			);
		} else if (managementType == MANAGEMENT_NOTIFY) {
			emberSerialPrintf(0, "%.*s", length, message);
		} else {
			syslog(LOG_ERR, "[NCP->] Unknown frame type %d!\n", managementType);
		}
	} else if (gNCPInstance->should_forward_hostbound_frame(&type_as_byte, message, length)) {
		switch(type) {
		case UART_LINK_TYPE_UNSECURED_DATA:
		case UART_LINK_TYPE_THREAD_DATA:
			gNCPInstance->handle_normal_ipv6_from_ncp(message, length);
			break;
		case UART_LINK_TYPE_ALT_DATA:
			gNCPInstance->handle_alt_ipv6_from_ncp(message, length);
			break;
		default:
			break;
		}
	}
}

void emReallySendManagementCommand(const uint8_t *command, uint16_t length)
{
	gNCPInstance->send_management_command(nl::Data(command+5,length-5));
}

EmberStatus emberSerialPrintfVarArg(uint8_t port, PGM_P formatString, va_list list)
{
#if USE_SYSLOG_FOR_EMBER_DEBUG_PRINTS
	static char buffer[512];
	static int buffer_len = 0;
	buffer_len += vsnprintf(buffer+buffer_len, sizeof(buffer)-buffer_len, formatString, list);

	char* line = buffer+sizeof(buffer)-1;
	while((buffer_len==sizeof(buffer)-1) || (line = strchr(buffer,'\n'))) {
		line[0] = 0;
		syslog(LOG_INFO, "SILABS: %s", buffer);
		memmove(buffer,line+1,strlen(line+1)+1);
		buffer_len = (int)strlen(buffer);
	}
	return EMBER_SUCCESS;
#else
	return vfprintf(stderr, formatString, list) < 0
		? EMBER_NO_BUFFERS
		: EMBER_SUCCESS;
#endif
}

EmberStatus emberSerialPrintf(int8u port, PGM_P formatString, ...)
{
	va_list list;
	va_start(list, formatString);
	emberSerialPrintfVarArg(port, formatString, list);
	va_end(list);
	return EMBER_SUCCESS;
}

EmberStatus emberSerialPrintfLine(int8u port, PGM_P formatString, ...)
{
	EmberStatus ret;
	va_list list;
	va_start(list, formatString);
	emberSerialPrintf(port, formatString, list);
	ret = emberSerialPrintf(port, "\n");
	va_end(list);
	return ret;
}

wpantund_status_t
EmberGlue::status_to_wpantund_status(EmberStatus status)
{
	switch(status) {
	case EMBER_SUCCESS: return kWPANTUNDStatus_Ok;
	case EMBER_ERR_FATAL: return kWPANTUNDStatus_NCP_Fatal;
	case EMBER_INVALID_CALL: return kWPANTUNDStatus_InvalidForCurrentState;
	case EMBER_JOIN_FAILED: return kWPANTUNDStatus_JoinFailedUnknown;
	case EMBER_OPERATION_IN_PROGRESS: return kWPANTUNDStatus_InProgress;
	case EMBER_BAD_ARGUMENT: return kWPANTUNDStatus_NCP_InvalidArgument;
	case EMBER_INDEX_OUT_OF_RANGE: return kWPANTUNDStatus_NCP_InvalidRange;

	default: return WPANTUND_NCPERROR_TO_STATUS(status);
	}

}

wpantund_status_t
ember_join_failure_reason_to_wpantund_status(EmberJoinFailureReason reason)
{
	switch(reason) {
	case EMBER_JOIN_FAILURE_REASON_ACTIVE_SCAN: return kWPANTUNDStatus_JoinFailedAtScan;
	case EMBER_JOIN_FAILURE_REASON_FORM_SCAN: return kWPANTUNDStatus_FormFailedAtScan;
	case EMBER_JOIN_FAILURE_REASON_NONE: return kWPANTUNDStatus_Ok;
	case EMBER_JOIN_FAILURE_REASON_SECURITY: return kWPANTUNDStatus_JoinFailedAtAuthenticate;

	default:
		return WPANTUND_NCPERROR_TO_STATUS(0x8000 + reason);
	}
}



void
emberStateReturn(
	const EmberNetworkParameters *parameters,
	const EmberEui64 *eui64,
	const EmberEui64 *extendedMacId,
	EmberNetworkStatus networkStatus
) {
	gNCPControlInterface->handle_ncp_network_state_changed();

	gNCPControlInterface->process_event(EVENT_EMBER_CB_STATE_RETURN, parameters, eui64, extendedMacId, networkStatus);
}

void emberInitReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_INIT_RETURN, status);
}

void emberEnergyScanHandler(uint8_t channel, int8_t maxRssiValue)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_ENERGY_SCAN_HANDLER, channel, maxRssiValue);
}

void emberScanReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SCAN_RETURN, status);
}

void emberActiveScanHandler(const EmberMacBeaconData *beaconData)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_ACTIVE_SCAN_HANDLER, beaconData);
}
void emberSetSecurityParametersReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SET_SECURITY_PARAMETERS_RETURN, status);
}

void emberSwitchToNextNetworkKeyReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SWITCH_TO_NEXT_NETWORK_KEY_RETURN, status);
}

void emberGetNetworkKeyInfoReturn(EmberStatus status,
                                  uint32_t sequence,
                                  uint8_t state)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_NETWORK_KEY_INFO_RETURN, status, sequence, state);
}

void emberCommissionNetworkReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_COMMISSION_NETWORK_RETURN, status);
}

void emberFormNetworkReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_FORM_NETWORK_RETURN, status);
}

void emberJoinNetworkReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_JOIN_NETWORK_RETURN, status);
}

void emberAttachToNetworkReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_ATTACH_TO_NETWORK_RETURN, status);
}

void emberResumeNetworkReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_RESUME_NETWORK_RETURN, status);
}


void emberGetRoutingLocatorReturn(const EmberIpv6Address *rloc)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_ROUTING_LOCATOR_RETURN, rloc);
}

void emberConfigureGatewayReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_CONFIGURE_GATEWAY_RETURN, status);
}

void emberConfigureExternalRouteReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_CONFIGURE_EXTERNAL_ROUTE_RETURN, status);
}

void emberGetChannelCalDataTokenReturn(uint8_t lna,
                                       int8_t tempAtLna,
                                       uint8_t modDac,
                                       int8_t tempAtModDac)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_CHANNEL_CAL_DATA_TOKEN_RETURN, lna, tempAtLna, modDac, tempAtModDac);
}

void emberGetRipEntryReturn(uint8_t index, const EmberRipEntry *entry)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_RIP_ENTRY_RETURN, index, entry);
}

void emberGetMulticastEntryReturn(uint8_t lastSequence,
                                  uint8_t windowBitmask,
                                  uint8_t dwellQs,
                                  const uint8_t *seed)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_MULTICAST_ENTRY_RETURN, lastSequence,windowBitmask,dwellQs,seed);
}

void emberGetCounterReturn(EmberCounterType type, uint16_t value)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_COUNTER_RETURN, type, value);
}

void emberSetTxPowerModeReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SET_TX_POWER_MODE_RETURN, status);
}

void emberGetTxPowerModeReturn(uint16_t txPowerMode)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_TX_POWER_MODE_RETURN, txPowerMode);
}

void emberGetVersionsReturn(const uint8_t *versionName,
                            uint16_t binaryManagementVersionNumber,
                            uint16_t stackVersionNumber,
                            uint16_t stackBuildNumber,
                            EmberVersionType versionType,
                            const uint8_t *buildTimestamp)
{
	gNCPControlInterface->handle_ncp_get_versions(versionName,binaryManagementVersionNumber, stackVersionNumber, stackBuildNumber, versionType, buildTimestamp);
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_VERSIONS_RETURN, versionName, binaryManagementVersionNumber, stackVersionNumber, stackBuildNumber, versionType, buildTimestamp);
}

void emberResetMicroHandler(EmberResetCause cause)
{
	gNCPControlInterface->handle_ncp_reset(cause);
	gNCPControlInterface->process_event(EVENT_EMBER_CB_RESET_MICRO_HANDLER, cause);
}

void emberResetNetworkStateReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_RESET_NETWORK_STATE_RETURN, status);
}

void emberGetCcaThresholdReturn(int8_t threshold)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_CCA_THRESHOLD_RETURN, threshold);
}

void emberSetCcaThresholdReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SET_CCA_THRESHOLD_RETURN, status);
}

void emberGetRadioPowerReturn(int8_t power)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_RADIO_POWER_RETURN, power);
}

void emberSetRadioPowerReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SET_RADIO_POWER_RETURN, status);
}

void emberAddAddressDataReturn(uint16_t shortId)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_ADD_ADDRESS_DATA_RETURN, shortId);
}

void emberClearAddressCacheReturn(void)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_CLEAR_ADDRESS_CACHE_RETURN, 0);
}

void emberLookupAddressDataReturn(uint16_t shortId)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_LOOKUP_ADDRESS_DATA_RETURN, shortId);
}

void emberListenForWakeupReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_LISTEN_FOR_WAKEUP_RETURN, status);
}

void emberSleepNoTimerReturn(void)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SLEEP_NO_TIMER_RETURN, 0);
}

void emberWakeupStateReturn(uint8_t wakeupState,
                            uint16_t wakeupSequenceNumber)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_WAKEUP_STATE_RETURN, wakeupState, wakeupSequenceNumber);
}

void emberStartWakeupReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_START_WAKEUP_RETURN, status);
}

void emberAbortWakeupReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_ABORT_WAKEUP_RETURN, status);
}

void emberActiveWakeupReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_ACTIVE_WAKEUP_RETURN, status);
}

void emberWakeupHandler(EmberWakeupReason reason,
                        EmberWakeupState state,
                        uint16_t remainingMs,
                        uint8_t dataByte,
                        uint16_t otaSequence)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_WAKEUP_HANDLER, reason, state, remainingMs, dataByte, otaSequence);
}

void emberSetWakeupSequenceNumberReturn(void)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SET_WAKEUP_SEQUENCE_NUMBER_RETURN, 0);
}

void emberAssertInfoReturn(const uint8_t *fileName, uint32_t lineNumber)
{
	gNCPControlInterface->handle_ncp_assert(fileName, lineNumber);
	gNCPControlInterface->process_event(EVENT_EMBER_CB_ASSERT_INFO_RETURN, fileName, lineNumber);
}

void emberPollForDataReturn(EmberStatus status, bool dataPending)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_POLL_FOR_DATA_RETURN, status, dataPending);
}

void emberDeepSleepReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_DEEP_SLEEP_RETURN, status);
}

void emberDeepSleepCompleteHandler(uint16_t sleepDuration)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_DEEP_SLEEP_CALLBACK, sleepDuration);
}

void emberStackPollForDataReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_STACK_POLL_FOR_DATA_RETURN, status);
}

void emberOkToNapReturn(uint8_t stateMask)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_OK_TO_NAP_RETURN, stateMask);
}


void emberNcpUdpStormReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_NCP_UDP_STORM_RETURN, status);
}

void emberNcpUdpStormCompleteHandler(void)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_NCP_UDP_STORM_COMPLETE, 0);
}

void mfglibStartReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_START_RETURN, status);
}

void mfglibEndReturn(EmberStatus status, uint32_t receiveCount)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_END_RETURN, status, receiveCount);
}

void mfglibStartToneReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_START_TONE_RETURN, status);
}

void mfglibStopToneReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_STOP_TONE_RETURN, status);
}

void mfglibStartStreamReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_START_STREAM_RETURN, status);
}

void mfglibStopStreamReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_STOP_STREAM_RETURN, status);
}

void mfglibSendPacketReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_SEND_PACKET_RETURN, status);
}

void mfglibSetChannelReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_SET_CHANNEL_RETURN, status);
}

void mfglibGetChannelReturn(uint8_t channel)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_GET_CHANNEL_RETURN, channel);
}

void mfglibSetPowerReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_SET_POWER_RETURN, status);
}

void mfglibGetPowerReturn(int8_t power)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_GET_POWER_RETURN, power);
}

void mfglibGetPowerModeReturn(uint16_t txPowerMode)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_GET_POWER_MODE_RETURN, txPowerMode);
}

void mfglibGetSynOffsetReturn(int8_t synthOffset)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_GET_SYN_OFFSET_RETURN, synthOffset);
}


void emberEchoReturn(const uint8_t *data, uint8_t length)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_ECHO_RETURN, data, length);
}

void emberResetNcpAshReturn(void)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_RESET_NCP_ASH_RETURN, 0);
}


void emberGetNodeStatusReturn(EmberStatus status,
                              uint8_t ripId,
                              EmberNodeId nodeId,
                              uint8_t parentRipId,
                              EmberNodeId parentId,
                              const uint8_t *networkFragmentIdentifier,
                              uint32_t networkFrameCounter)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_NODE_STATUS_RETURN, status, ripId, nodeId, parentRipId, parentId, networkFragmentIdentifier,networkFrameCounter);
}

void emberGetGlobalPrefixReturn(uint8_t borderRouterFlags,
                                bool isStable,
                                const uint8_t *prefix,
                                uint8_t domainId,
                                uint32_t preferredLifetime,
                                uint32_t validLifetime)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_GLOBAL_PREFIX_RETURN, borderRouterFlags, prefix, domainId, preferredLifetime, validLifetime);
}

void emberGetGlobalAddressReturn(const EmberIpv6Address *address,
                                 uint32_t preferredLifetime,
                                 uint32_t validLifetime,
                                 uint8_t flags)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_GLOBAL_ADDRESS_RETURN, address, preferredLifetime, validLifetime, flags);
}

void emberGetDhcpClientReturn(const EmberIpv6Address *address)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_DHCP_CLIENT_RETURN, address);
}

void emberConfigUartReturn(void)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_CONFIG_UART_RETURN, 0);
}

void emberExternalRouteChangeHandler(const uint8_t *prefix,
                              uint8_t prefixLength,
                              bool available)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_EXTERNAL_ROUTE_CHANGE, prefix, prefixLength, available);
}

void emberDhcpServerChangeHandler(const uint8_t *prefix,
                           uint8_t prefixLength,
                           bool available)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_DHCP_SERVER_CHANGE, prefix, prefixLength, available);
}

void emberSlaacServerChangeHandler(const uint8_t *prefix,
                            uint8_t prefixLength,
                            bool available)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SLAAC_SERVER_CHANGE, prefix, prefixLength, available);
}

void emberAddressConfigurationChangeHandler(const EmberIpv6Address *address,
                                     uint32_t preferredLifetime,
                                     uint32_t validLifetime,
                                     uint8_t addressFlags)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_ADDRESS_CONFIGURATION_CHANGE, address, preferredLifetime, validLifetime, addressFlags);
}

void emberRequestDhcpAddressReturn(EmberStatus status,
                                   const uint8_t *prefix,
                                   uint8_t prefixLength)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_REQUEST_DHCP_ADDRESS_RETURN, status, prefix, prefixLength);
}

void emberRequestSlaacAddressReturn(EmberStatus status,
                                    const uint8_t *prefix,
                                    uint8_t prefixLength)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_REQUEST_SLAAC_ADDRESS_RETURN, status, prefix, prefixLength);
}

void emberResignGlobalAddressReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_RESIGN_GLOBAL_ADDRESS_RETURN, status);
}

void emberUartSpeedTestReturn(uint32_t totalBytesSent,
                              uint32_t payloadBytesSent,
                              uint32_t timeLengthMs)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_UART_SPEED_TEST_RETURN, totalBytesSent, payloadBytesSent, timeLengthMs);
}

void emberCommissionerStatusHandler(uint16_t flags,
                                    const uint8_t *commissionerName,
                                    uint8_t commissionerNameLength)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_COMMISSIONER_STATUS_HANDLER, flags, commissionerName, commissionerNameLength);
}

void emberBecomeCommissionerReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_BECOME_COMMISSIONER_RETURN, status);
}

void emberSendSteeringDataReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SEND_STEERING_DATA_RETURN, status);
}


void emberChangeNodeTypeReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_CHANGE_NODE_TYPE_RETURN, status);
}

void emberStartUartStormReturn(void)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_START_UART_STORM_RETURN, 0);
}

void emberStopUartStormReturn(void)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_STOP_UART_STORM_RETURN, 0);
}

void emberGpioCurrentConfigReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GPIO_CURRENT_CONFIG_RETURN, status);
}

void emberGpioCurrentInputReturn(uint8_t value)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GPIO_CURRENT_INPUT_RETURN, value);
}

void emberGpioPowerConfigReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GPIO_POWER_CONFIG_RETURN, status);
}

void emberPermitJoiningReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_PERMIT_JOINING_RETURN, status);
}

void emberPermitJoiningHandler(bool joiningAllowed)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_PERMIT_JOINING_HANDLER, joiningAllowed);
}

void emberLeaderDataHandler(const uint8_t *leaderData)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_LEADER_DATA_HANDLER, leaderData);
}

void emberGetNetworkDataTlvReturn(uint8_t typeByte,
                                  uint8_t index,
                                  uint8_t versionNumber,
                                  const uint8_t *tlv,
                                  uint8_t tlvLength)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_NETWORK_DATA_TLV_RETURN, typeByte, index, versionNumber, tlv, tlvLength);
}


void emberStartHostJoinClientHandler(const uint8_t *parentAddress)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_START_HOST_JOIN_CLIENT_HANDLER, parentAddress);
}

void emInitializeBuffers(void)
{
	syslog(LOG_WARNING,"emInitializeBuffers");
}

EmberNodeId emberGetNodeId(void) { return EM_USE_LONG_ADDRESS; }

void emberConfigureDefaultHostAddress(const EmberIpv6Address *mlAddress)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_CONFIGURE_DEFAULT_HOST_ADDRESS, mlAddress);
}

void emberConfigureLegacyHostAddress(const EmberIpv6Address *mlAddress)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_CONFIGURE_LEGACY_HOST_ADDRESS, mlAddress);
}

void emberConfigureGlobalHostAddress(const EmberIpv6Address *address,
                                     uint32_t preferredLifetime,
                                     uint32_t validLifetime,
                                     uint8_t addressFlags)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_CONFIGURE_GLOBAL_HOST_ADDRESS, address, preferredLifetime, validLifetime, addressFlags);
}

void mfglibRxHandler(uint8_t *packet,
                     uint8_t linkQuality,
                     int8_t rssi)
{
	gNCPControlInterface->handle_cb_mfg_rx(packet, linkQuality, rssi);
	gNCPControlInterface->process_event(EVENT_EMBER_CB_MFGLIB_RX_HANDLER, packet, linkQuality, rssi);
}

void nestEmberSetCoexParametersReturn(uint8_t status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SET_COEX_PARAMETERS_RETURN, status);
}

void nestEmberGetCoexStatsReturn(uint8_t parameters, uint8_t reserved1, uint8_t state, uint8_t reserved2)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_COEX_STATS_RETURN, parameters, reserved1, state, reserved2);
}

void nestEmberSetTransmitHookActiveReturn()
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SET_TRANSMIT_HOOK_ACTIVE_RETURN, 0);
}

void emberSetCommAppSecurityHandler(const uint8_t *masterKey,
                                    uint32_t sequenceNumber)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SET_NETWORK_KEYS_HANDLER, sequenceNumber, masterKey, 0, NULL);
}

void emberSetNetworkKeysHandler(uint32_t sequence,
                                const uint8_t *masterKey,
                                uint32_t sequence2,
                                const uint8_t *masterKey2)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SET_NETWORK_KEYS_HANDLER, sequence,masterKey, sequence2, masterKey2);
}





void emberNetworkStatusHandler(EmberNetworkStatus newNetworkStatus,
                               EmberNetworkStatus oldNetworkStatus,
							   EmberJoinFailureReason reason)
{
	gNCPControlInterface->handle_ncp_network_state_changed();

	gNCPControlInterface->process_event(
		EVENT_EMBER_CB_NETWORK_STATUS_HANDLER,
		newNetworkStatus,
		oldNetworkStatus,
		reason
	);
}


void emberAllowNativeCommissionerReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_ALLOW_NATIVE_COMMISSIONER_RETURN, status);
}

void emberSetCommissionerKeyReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SET_COMMISSIONER_KEY_RETURN, status);
}

void emberSetRandomizeMacExtendedIdReturn(void)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SET_RANDOMIZE_MAC_EXTENDED_ID_RETURN, 0);
}

void emberGetMfgTokenReturn(EmberMfgTokenId tokenId,
                            EmberStatus status,
                            const uint8_t *tokenData,
                            uint8_t tokenDataLength)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_MFG_TOKEN_RETURN, tokenId, status, tokenData, tokenDataLength);
}

void emberSetMfgTokenReturn(EmberMfgTokenId tokenId,
                            EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_SET_MFG_TOKEN_RETURN, tokenId, status);
}

void emberGetStandaloneBootloaderInfoReturn(uint16_t version,
                                            uint8_t platformId,
                                            uint8_t microId,
                                            uint8_t phyId)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_GET_STANDALONE_BOOTLOADER_INFO_RETURN, version, platformId, microId, phyId);
}

void emberLaunchStandaloneBootloaderReturn(EmberStatus status)
{
	gNCPControlInterface->process_event(EVENT_EMBER_CB_LAUNCH_STANDALONE_BOOTLOADER_RETURN, status);
}

void emberSetLegacyFilterMaxThresholdReturn(EmberStatus status)
{
	// Ignore for now.
}

void emberSetLegacyFilterMinThresholdReturn(EmberStatus status)
{
	// Ignore for now.
}
