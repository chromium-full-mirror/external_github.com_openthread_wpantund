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
 *    Description:
 *      This file contains an implementation of the FirmwareInterface
 *      class for NCP stacks running Thread/2.0.x.
 *
 *  @author Robert Quattelbaum
 *  @author Martin Turon
 *  @date   April 13, 2015
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include "FirmwareInterface-Thread-1.0.0.h"
#include <boost/algorithm/string/predicate.hpp>
#include <errno.h>

using namespace nl;
using namespace wpantund;
using namespace EmberCommands;

// ------------------------------------------------------------------------------
// MARK: Constants and Enums
// ------------------------------------------------------------------------------

#include "silabs-thread-1.0/stack/include/counter.h"
#include "silabs-thread-1.0/stack/include/error.h"
#include "silabs-thread-1.0/app/tmsp/tmsp-enum.h"
#include "silabs-thread-1.0/app/tmsp/tmsp-custom-enum.h"

enum {
	EMBER_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER   = 0x69AA,
};

#define BIT(x)		(1<<(x))

#define EMBER_USE_DEFAULTS            0
#define EMBER_NETWORK_ID_OPTION       BIT(0)
#define EMBER_ULA_PREFIX_OPTION       BIT(1)
#define EMBER_EXTENDED_PAN_ID_OPTION  BIT(2)
#define EMBER_PAN_ID_OPTION           BIT(3)
#define EMBER_NODE_TYPE_OPTION        BIT(4)
#define EMBER_TX_POWER_OPTION         BIT(5)
#define EMBER_MASTER_KEY_OPTION       BIT(6)
#define EMBER_LEGACY_ULA_OPTION       BIT(7)
#define EMBER_JOIN_KEY_OPTION         BIT(8)

static const char* counter_cstr[] = { EMBER_COUNTER_STRINGS };

// ------------------------------------------------------------------------------
// MARK: Methods
// ------------------------------------------------------------------------------

FirmwareInterface_Thread_1_0_0::FirmwareInterface_Thread_1_0_0(int management_version, const std::string& version):
	FirmwareInterface_2_0_0(management_version, version)
{
}

boost::shared_ptr<FirmwareInterface>
FirmwareInterface_Thread_1_0_0::alloc(int management_version, const std::string& version_string)
{
	return boost::shared_ptr<FirmwareInterface>(new FirmwareInterface_Thread_1_0_0(management_version, version_string));
}

wpantund_status_t
FirmwareInterface_Thread_1_0_0::NCPErrorToStatus(uint8_t errorcode)
{
	switch(errorcode) {
	case EMBER_INVALID_CALL: return kWPANTUNDStatus_InvalidForCurrentState;
	case EMBER_JOIN_FAILED: return kWPANTUNDStatus_JoinFailedUnknown;
	default: break;
	}

	return errorcode != 0
		? WPANTUND_NCPERROR_TO_STATUS(errorcode)
		: kWPANTUNDStatus_Ok;
}

Data
FirmwareInterface_Thread_1_0_0::CMD_JoinCommissioned(int8_t tx_power, uint8_t node_type)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "suu",
		EMBER_JOIN_COMMISSIONED_COMMAND_IDENTIFIER,
		tx_power,
		node_type,
		0 /* Require Connectivity Flag */
	);
}

Data
FirmwareInterface_Thread_1_0_0::CMD_Join(const WPAN::NetworkInstance& network, uint8_t node_type, int tx_power)
{
	char network_id[16];
	uint32_t channel_mask = kAllChannelsMask;

	uint16_t options = EMBER_NETWORK_ID_OPTION;

	if ((abs(tx_power) & ~0x7F) == 0) {
		options |= EMBER_TX_POWER_OPTION;
	} else {
		tx_power = 0;
	}

	// Coverty might complain here with some ominous warning about
	// `network_id` being non-NULL terminated. This is not a bug.
	// `network_id` doesn't need to be NULL-terminated.
	strncpy(network_id, network.name.c_str(), sizeof(network_id));

	if (node_type != UNKNOWN) {
		options |= EMBER_NODE_TYPE_OPTION;
	}

	if (network.panid != 0xFFFF) {
		options |= EMBER_PAN_ID_OPTION;
	}

	if (network.get_xpanid_as_uint64() != 0) {
		options |= EMBER_EXTENDED_PAN_ID_OPTION;
	}

	if (network.channel != 0) {
		channel_mask = (1 << network.channel);
	}

	return PackArguments(
		EMBER_PACKED_UINT16 "bbvusbuvw",
		EMBER_JOIN_NETWORK_COMMAND_IDENTIFIER,
		sizeof(network_id),
		network_id,
		sizeof(network.xpanid),
		network.xpanid,
		network.panid,
		node_type,
		tx_power,

		0, NULL, // "Join Key"
		0,       // "Join Key Length"

		options,
		channel_mask
	);
}

Data
FirmwareInterface_Thread_1_0_0::CMD_CommissionNetwork(
	const WPAN::NetworkId& network,
	uint8_t perf_chan,
	uint32_t channel_mask,
	const uint8_t ula[8],
	const uint8_t network_key[16]
) {
	char network_id[16];

	// Coverty might complain here with some ominous warning about
	// `network_id` being non-NULL terminated. This is not a bug.
	// `network_id` doesn't need to be NULL-terminated.
	strncpy(network_id, network.name.c_str(), sizeof(network_id));

	int16_t panid = 0xFFFF;     // default to detect
	int32_t key_seq = 0;   // default to detect

	return PackArguments(
		EMBER_PACKED_UINT16 "uwbvbbbw",
		EMBER_COMMISSION_NETWORK_COMMAND_IDENTIFIER,
		perf_chan,
		channel_mask,
		sizeof(network_id), network_id,
		panid,
		ula?8:0, ula,
		sizeof(network.xpanid), network.xpanid,
		network_key?16:0, network_key,
		key_seq
	);
}

int
FirmwareInterface_Thread_1_0_0::Parse_CBState(
	const Data& data,
	WPAN::NetworkInstance* network,
	int8_t* tx_power,
	uint8_t* raw_type,
	uint8_t* association_state,
	uint8_t hwaddr[8],
	uint8_t mesh_local_prefix[8],
	uint8_t legacy_prefix[8]
) {
	int ret = FirmwareInterface_1_1_3::Parse_CBState(data, network, tx_power, raw_type, association_state, hwaddr, mesh_local_prefix, legacy_prefix);;

	require(ret != -1, bail);

	if ((legacy_prefix != NULL) && (GetManagementVersion() > 9)) {
		int legacy_prefix_size = 0;
		const uint8_t* legacy_prefix_data = NULL;

		ret = EmberCommands::UnpackArguments(
			data,
			EMBER_PACKED_UINT16 "bbbvuusubb",
			NULL, // Skip command identifier
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			&legacy_prefix_size,
			&legacy_prefix_data
		);

		require(ret != -1, bail);
		require_action(legacy_prefix_size == 8, bail, ret = -1);

		memcpy(legacy_prefix, legacy_prefix_data, 8);
	}

bail:
	return ret;
}

bool
FirmwareInterface_Thread_1_0_0::Workaround_CommissionNetworkRequired(void)
{
	return true;
}

Data
FirmwareInterface_Thread_1_0_0::CMD_Scan(int period, uint32_t channel_mask)
{
	enum {
		SCAN_ENERGY,
		SCAN_ACTIVE
	};

	return PackArguments(
		EMBER_PACKED_UINT16 "uwu",
		EMBER_START_SCAN_COMMAND_IDENTIFIER,
		SCAN_ACTIVE,
		channel_mask,
		period
	);
}

int FirmwareInterface_Thread_1_0_0::GetIDFor_CBConfigGatewayReturn() { return CB_CONFIGURE_GATEWAY_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBMfgEndTest(void) { return CB_MFGLIB_END_TEST_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBActiveScan() { return CB_ACTIVE_SCAN_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBAssertInfo() { return CB_ASSERT_INFO_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBCounters() { return CB_GET_COUNTER_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBDataPoll() { return CB_POLL_FOR_DATA_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBEcho() { return CB_ECHO_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBForm() { return CB_FORM_NETWORK_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBGetCCAThreshold() { return CB_GET_CCA_THRESHOLD_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBGetChannelCalToken() { return CB_GET_CHANNEL_CAL_DATA_TOKEN_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBGetTXPowerMode() { return CB_GET_TX_POWER_MODE_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBJoin() { return CB_JOIN_NETWORK_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBListen() { return CB_LISTEN_FOR_WAKEUP_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBMfgGetChannel() { return CB_MFGLIB_GET_CHANNEL_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBMfgGetPower() { return CB_MFGLIB_GET_POWER_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBMfgGetPowerMode() { return CB_MFGLIB_GET_POWER_MODE_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBMfgGetSynOffset() { return CB_MFGLIB_GET_SYN_OFFSET_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBMfgRX() { return CB_MFGLIB_RX_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBMfgSendPacket() { return CB_MFGLIB_SEND_PACKET_EVENT_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBMfgSet() { return CB_MFGLIB_SET_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBMfgStart() { return CB_MFGLIB_START_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBMfgStartTest() { return CB_MFGLIB_START_TEST_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBMfgStop() { return CB_MFGLIB_STOP_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBOKToNap() { return CB_OK_TO_NAP_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBPermitJoin() { return CB_PERMIT_JOINING_HANDLER_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBPermitJoinReturn() { return CB_PERMIT_JOINING_RETURN_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBReset() { return CB_RESET_MICRO_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBResume() { return CB_RESUME_NETWORK_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBRIPEntry() { return CB_GET_RIP_ENTRY_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBScanReturn() { return CB_SCAN_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBSetCCAThreshold() { return CB_SET_CCA_THRESHOLD_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBSetRadioPower() { return CB_SET_RADIO_POWER_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBSetSecurityParams() { return CB_SET_SECURITY_PARAMETERS_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBSetTXPowerMode() { return CB_SET_TX_POWER_MODE_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBSleepNoTimer() { return CB_SLEEP_NO_TIMER_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBStackDataPoll() { return CB_STACK_POLL_FOR_DATA_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBStartHostJoinClient() { return CB_START_HOST_JOIN_CLIENT_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBStartWakeup() { return CB_START_WAKEUP_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBState() { return CB_STATE_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBWakeup() { return CB_WAKEUP_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBGPIOCurrentConfig() { return CB_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBGPIOCurrentInput() { return CB_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBCustomNCPToHostMessage() { return CB_CUSTOM_NCP_TO_HOST_MESSAGE_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBAddressConfigurationChange() { return CB_ADDRESS_CONFIGURATION_CHANGE_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBChangeNodeType() { return CB_CHANGE_NODE_TYPE_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBRequestSLAACAddress() { return CB_REQUEST_SLAAC_ADDRESS_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBSLAACServerChange() { return CB_SLAAC_SERVER_CHANGE_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBSetDriverAddress() { return CB_SET_DRIVER_ADDRESS_COMMAND_IDENTIFIER; }
int FirmwareInterface_Thread_1_0_0::GetIDFor_CBResignGlobalAddress() { return 0; }

Data
FirmwareInterface_Thread_1_0_0::CMD_ResignGlobalAddress(const struct in6_addr* address)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "b",
		EMBER_RESIGN_GLOBAL_ADDRESS_COMMAND_IDENTIFIER,
		sizeof(*address),
		address->s6_addr
	);
}

Data
FirmwareInterface_Thread_1_0_0::CMD_RequestSLAACAddress(const uint8_t prefix[], int prefix_len)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "b",
		EMBER_REQUEST_SLAAC_ADDRESS_COMMAND_IDENTIFIER,
		prefix_len,
		prefix
	);
}

int
FirmwareInterface_Thread_1_0_0::Parse_CBSLAACServerChange(const Data& data, const uint8_t **prefix, int *prefix_len, uint8_t *available)
{
	return EmberCommands::UnpackArguments(
		data,
		EMBER_PACKED_UINT16 "bu",
		NULL, // Skip command identifier
		prefix_len,
		prefix,
		available
	);
}

int
FirmwareInterface_Thread_1_0_0::Parse_CBAddressConfigurationChange(const Data& data, struct in6_addr* address, uint32_t* valid_lifetime, uint32_t* preferred_lifetime, uint8_t* flags)
{
	int ret;
	int addr_len(0);
	const uint8_t* addr_raw(NULL);
	ret = EmberCommands::UnpackArguments(
		data,
		EMBER_PACKED_UINT16 "bwwu",
		NULL, // Skip command identifier
		&addr_len,
		&addr_raw,
		valid_lifetime,
		preferred_lifetime,
		flags
	);

	if (ret >= 0) {
		if (addr_len == sizeof(struct in6_addr)) {
			memcpy(address->s6_addr, addr_raw, addr_len);
		} else {
			ret = -1;
		}
	}

	return ret;
}

Data
FirmwareInterface_Thread_1_0_0::CMD_ConfigGateway(uint8_t flags, const uint8_t prefix[], size_t prefix_len, uint32_t preferredLifetime, uint32_t validLifetime, uint8_t domain_id)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "ubuww",
		EMBER_CONFIGURE_GATEWAY_COMMAND_IDENTIFIER,
		flags,
		prefix_len,
		prefix,
		domain_id,
		preferredLifetime,
		validLifetime
	);
}

Data
FirmwareInterface_Thread_1_0_0::CMD_PermitJoin(int seconds, bool network_wide)
{
	(void)network_wide;
	return PackArguments(
		EMBER_PACKED_UINT16 "v",
		EMBER_PERMIT_JOINING_COMMAND_IDENTIFIER,
		seconds
	);
}

std::string
FirmwareInterface_Thread_1_0_0::CounterIndexToString(int x)
{
	std::string ret;

	if (x < 0) {
		goto bail;
	}

	if (x >= FirmwareInterface_Thread_1_0_0::GetCounterCount()) {
		goto bail;
	}

	ret = CounterIndexToString_Helper(x, counter_cstr);

bail:
	if (ret.empty()) {
		ret = FirmwareInterface_Base::CounterIndexToString(x);
	}

	return ret;
}

int
FirmwareInterface_Thread_1_0_0::GetCounterCount()
{
	// minus 1 because the list is NULL-terminated.
	return sizeof(counter_cstr)/sizeof(counter_cstr[0])-1;
}

Data
FirmwareInterface_Thread_1_0_0::CMD_SetTransmitHookActive(bool using_transmit_hook)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER,
		(uint8_t)using_transmit_hook
	);
}

static bool
IsCustomCommand(int cmdid)
{
	bool ret = false;

	switch(cmdid) {
	case EMBER_PERMIT_JOINING_COMMAND_IDENTIFIER:
	case EMBER_SET_PASSTHROUGH_PORT_COMMAND_IDENTIFIER:
	case EMBER_SET_CLOCK_MONITOR_COMMAND_IDENTIFIER:
	case EMBER_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER:
	case EMBER_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER:
	case EMBER_GPIO_POWER_CONFIG_COMMAND_IDENTIFIER:
	case EMBER_GPIO_RADIO_POWER_MASK_COMMAND_IDENTIFIER:
	case EMBER_CONFIGURE_WAKEUP_COMMAND_IDENTIFIER:
	case EMBER_SLEEP_NO_TIMER_COMMAND_IDENTIFIER:
	case EMBER_LISTEN_FOR_WAKEUP_COMMAND_IDENTIFIER:
	case EMBER_START_WAKEUP_COMMAND_IDENTIFIER:
	case EMBER_ABORT_WAKEUP_COMMAND_IDENTIFIER:
	case EMBER_WAKEUP_STATE_COMMAND_IDENTIFIER:
	case EMBER_ACTIVE_WAKEUP_COMMAND_IDENTIFIER:
	case EMBER_SET_WAKEUP_SEQUENCE_NUMBER_COMMAND_IDENTIFIER:
	case EMBER_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER:
		ret = true;
		break;
	}
	return ret;
}

Data
FirmwareInterface_Thread_1_0_0::WrapCustomCommand(const Data& x)
{
	if (IsCustomCommand(FirmwareInterface_Base::GetCMDID(x.data(),x.size()))) {
		return PackArguments(
			EMBER_PACKED_UINT16 "b",
			EMBER_CUSTOM_HOST_TO_NCP_MESSAGE_COMMAND_IDENTIFIER,
			x.size(),
			x.data()
		);
	}
	return x;
}

int
FirmwareInterface_Thread_1_0_0::UnwrapCustomCallback(const uint8_t*&data, size_t&len)
{
	int cmdid(FirmwareInterface_Base::GetCMDID(data, len));

	switch(cmdid) {
	case EMBER_CUSTOM_HOST_TO_NCP_MESSAGE_COMMAND_IDENTIFIER:
	case CB_CUSTOM_NCP_TO_HOST_MESSAGE_COMMAND_IDENTIFIER:
		if (UnpackArguments(
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

Data
FirmwareInterface_Thread_1_0_0::CMD_GetGlobalAddresses(const uint8_t prefix[], int prefix_len)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "b",
		EMBER_GET_GLOBAL_ADDRESSES_COMMAND_IDENTIFIER,
		prefix_len,
		prefix
	);
}

int
FirmwareInterface_Thread_1_0_0::GetIDFor_CBGetGlobalAddress()
{
	return CB_GET_GLOBAL_ADDRESS_COMMAND_IDENTIFIER;
}

int
FirmwareInterface_Thread_1_0_0::Parse_CBGetGlobalAddress(const Data& data, struct in6_addr* address,
		uint32_t* preferred_lifetime, uint32_t* valid_lifetime, uint8_t* flags)
{
	int ret;
	int addr_len(0);
	const uint8_t* addr_raw(NULL);

	ret = EmberCommands::UnpackArguments(
		data,
		EMBER_PACKED_UINT16 "bwwu",
		NULL, // Skip command identifier
		&addr_len,
		&addr_raw,
		preferred_lifetime,
		valid_lifetime,
		flags
	);

	if (ret >= 0) {
		if (addr_len == sizeof(struct in6_addr)) {
			memcpy(address->s6_addr, addr_raw, addr_len);
		} else {
			ret = -1;
		}
	}

	return ret;
}

Data
FirmwareInterface_Thread_1_0_0::CMD_ChangeNodeType(uint8_t type)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_CHANGE_NODE_TYPE_COMMAND_IDENTIFIER,
		type
	);
}

int
FirmwareInterface_Thread_1_0_0::Parse_CBActiveScan(
	const Data& data,
	WPAN::NetworkInstance* network_instance
) {
	std::string version_string(GetVersionString());

	if (boost::starts_with(version_string,"Thread/1.0.1") || boost::starts_with(version_string,"Thread/1.0.0")) {
		// This new format only started with thread 1.0.2
		return FirmwareInterface_2_0_0::Parse_CBActiveScan(data, network_instance);
	}

	int i = 0;
	uint8_t permit_join = 0;
	uint8_t channel = 0;
	uint8_t lqi = 0;
	uint8_t rssi = 0;
	int network_id_size = 0;
	const char* network_id_data = NULL;
	uint16_t panid = 0xFFFF;
	int xpanid_size = 0;
	const uint8_t* xpanid_data = NULL;
	int source_size = 0;
	const uint8_t* source_data = NULL;
	uint8_t type = 0;

	if (EmberCommands::UnpackArguments(
		data.data() + 2,
		data.size() - 2,
        "uuuubvbbuubu",
		&permit_join,
		&channel,
		&lqi,
		&rssi,
		&network_id_size, &network_id_data,
		&panid,
		&xpanid_size, &xpanid_data,
		&source_size, &source_data,
		&type,
		NULL, // Protocol Version
		NULL, NULL, // Steering Data
		NULL // Steering Data Length
	) == -1) {
		return -errno;
	}

	char network_name[network_id_size+1];

	strncpy(network_name, network_id_data, network_id_size);

	// Make sure we zero terminate.
	network_name[network_id_size] = 0;

	// Strip any UTF-8 characters for now.
	// TODO: This should eventually allow *valid* UTF8 characters!
	for(i = 0; i < network_id_size; i++) {
		if ((network_name[i] & 0x80)
			|| ((network_name[i] != 0) && (network_name[i] < 32))
		) {
			network_name[i] = '?';
		}
	}

	WPAN::NetworkInstance network(
		network_name,
		xpanid_data,
		panid,
		channel,
		permit_join
	);

	network.rssi = rssi;
	network.type = type;

	if (source_size == 8) {
		memcpyrev(network.hwaddr, source_data, 8);
	}

	if (network_instance) {
		*network_instance = network;
	}
	return 0;
}

Data
FirmwareInterface_Thread_1_0_0::CMD_ChannelCalGet(uint8_t channel)
{
	static const uint8_t kChannelCalibrationDataTokenIndex = 0;

	return PackArguments(
			EMBER_PACKED_UINT16 "uu",
			EMBER_GET_INDEXED_TOKEN_COMMAND_IDENTIFIER,
			kChannelCalibrationDataTokenIndex,
			channel
	);
}


Data
FirmwareInterface_Thread_1_0_0::CMD_ConfigExternalRoute(uint8_t flags, const uint8_t route_prefix[], int prefix_len, uint8_t domain_id)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "ubu",
		EMBER_CONFIGURE_EXTERNAL_ROUTE_COMMAND_IDENTIFIER,
		flags,
		prefix_len, route_prefix,
		domain_id
	);
}

int
FirmwareInterface_Thread_1_0_0::GetIDFor_CBConfigExternalRoute()
{
	return CB_CONFIGURE_EXTERNAL_ROUTE_COMMAND_IDENTIFIER;
}

Data
FirmwareInterface_Thread_1_0_0::CMD_GetGlobalPrefixes()
{
	return PackArguments(
		EMBER_PACKED_UINT16,
		EMBER_GET_GLOBAL_PREFIXES_COMMAND_IDENTIFIER
	);
}

int
FirmwareInterface_Thread_1_0_0::GetIDFor_CBGetGlobalPrefix()
{
	return CB_GET_GLOBAL_PREFIX_COMMAND_IDENTIFIER;
}

int
FirmwareInterface_Thread_1_0_0::Parse_CBGetGlobalPrefix(const Data &data, uint8_t *flags, const uint8_t **prefix,
	uint8_t *domain_id, uint32_t *preferred_lifetime, uint32_t *valid_lifetime)
{
	int ret;
	int prefix_len;

	ret = UnpackArguments(
		data,
		EMBER_PACKED_UINT16 "ubuww",
		NULL,
		flags,
		&prefix_len, prefix,
		domain_id,
		preferred_lifetime,
		valid_lifetime
	);

	if (ret >= 0) {
		if (prefix_len != 8) {
			ret = -1;
		}
	}
	return ret;
}

std::string
FirmwareInterface_Thread_1_0_0::CommandIdentifierToString(uint16_t command_identifier)
{
	std::string ret;

#define HANDLE_COMMAND(x) case EMBER_##x##_COMMAND_IDENTIFIER: ret = #x "_COMMAND"; break;
#define HANDLE_CALLBACK(x) case CB_##x##_COMMAND_IDENTIFIER: ret = "CB_" #x; break;

	switch(command_identifier) {

	HANDLE_COMMAND(RESET_MICRO)
	HANDLE_COMMAND(RESET_NETWORK_STATE)
	HANDLE_COMMAND(INIT_HOST)
	HANDLE_COMMAND(STATE)
	HANDLE_COMMAND(GET_VERSIONS)
	HANDLE_COMMAND(FORM_NETWORK)
	HANDLE_COMMAND(JOIN_NETWORK)
	HANDLE_COMMAND(RESUME_NETWORK)
	HANDLE_COMMAND(ATTACH_TO_NETWORK)
	HANDLE_COMMAND(HOST_JOIN_CLIENT_COMPLETE)
	HANDLE_COMMAND(SET_SECURITY_PARAMETERS)
	HANDLE_COMMAND(SWITCH_TO_NEXT_NETWORK_KEY)
	HANDLE_COMMAND(START_SCAN)
	HANDLE_COMMAND(STOP_SCAN)
	HANDLE_COMMAND(GET_RIP_ENTRY)
	HANDLE_COMMAND(GET_MULTICAST_TABLE)
	HANDLE_COMMAND(GET_COUNTER)
	HANDLE_COMMAND(CLEAR_COUNTERS)
	HANDLE_COMMAND(SET_TX_POWER_MODE)
	HANDLE_COMMAND(GET_TX_POWER_MODE)
	HANDLE_COMMAND(FF_WAKEUP)
	HANDLE_COMMAND(GET_CCA_THRESHOLD)
	HANDLE_COMMAND(SET_CCA_THRESHOLD)
	HANDLE_COMMAND(GET_RADIO_POWER)
	HANDLE_COMMAND(SET_RADIO_POWER)
	HANDLE_COMMAND(ECHO)
	HANDLE_COMMAND(CONFIGURE_GATEWAY)
	HANDLE_COMMAND(GET_INDEXED_TOKEN)
	HANDLE_COMMAND(POLL_FOR_DATA)
	HANDLE_COMMAND(DEEP_SLEEP)
	HANDLE_COMMAND(STACK_POLL_FOR_DATA)
	HANDLE_COMMAND(OK_TO_NAP)
	HANDLE_COMMAND(PING)
	HANDLE_COMMAND(JOIN_COMMISSIONED)
	HANDLE_COMMAND(COMMISSION_NETWORK)
	HANDLE_COMMAND(REQUEST_DHCP_ADDRESS)
	HANDLE_COMMAND(HOST_TO_NCP_NO_OP)
	HANDLE_COMMAND(GET_GLOBAL_ADDRESSES)
	HANDLE_COMMAND(GET_DHCP_CLIENTS)
	HANDLE_COMMAND(SET_COMMISSIONING_MAC_KEY)
	HANDLE_COMMAND(ADD_STEERING_EUI64)
	HANDLE_COMMAND(BECOME_COMMISSIONER)
	HANDLE_COMMAND(GET_COMMISSIONER)
	HANDLE_COMMAND(SEND_STEERING_DATA)
	HANDLE_COMMAND(STOP_COMMISSIONING)
	HANDLE_COMMAND(SET_JOIN_KEY)
	HANDLE_COMMAND(SET_JOINING_MODE)
	HANDLE_COMMAND(CHANGE_NODE_TYPE)
	HANDLE_COMMAND(GET_GLOBAL_PREFIXES)
	HANDLE_COMMAND(RESIGN_GLOBAL_ADDRESS)
	HANDLE_COMMAND(SET_EUI64)
	HANDLE_COMMAND(REQUEST_SLAAC_ADDRESS)
	HANDLE_COMMAND(CUSTOM_HOST_TO_NCP_MESSAGE)
	HANDLE_COMMAND(MFGLIB_START)
	HANDLE_COMMAND(MFGLIB_END)
	HANDLE_COMMAND(MFGLIB_START_ACTIVITY)
	HANDLE_COMMAND(MFGLIB_STOP_ACTIVITY)
	HANDLE_COMMAND(MFGLIB_SEND_PACKET)
	HANDLE_COMMAND(MFGLIB_SET)
	HANDLE_COMMAND(MFGLIB_GET)
	HANDLE_COMMAND(MFGLIB_TEST_CONT_MOD_CAL)
	HANDLE_COMMAND(GET_NETWORK_KEY_INFO)
	HANDLE_COMMAND(GET_NODE_STATUS)
	HANDLE_COMMAND(ADD_ADDRESS_DATA)
	HANDLE_COMMAND(CLEAR_ADDRESS_CACHE)
	HANDLE_COMMAND(LOOKUP_ADDRESS_DATA)
	HANDLE_COMMAND(PERMIT_JOINING)
	HANDLE_COMMAND(SET_PASSTHROUGH_PORT)
	HANDLE_COMMAND(SET_CLOCK_MONITOR)
	HANDLE_COMMAND(GPIO_CURRENT_CONFIG)
	HANDLE_COMMAND(GPIO_CURRENT_INPUT)
	HANDLE_COMMAND(GPIO_POWER_CONFIG)
	HANDLE_COMMAND(GPIO_RADIO_POWER_MASK)
	HANDLE_COMMAND(CONFIGURE_WAKEUP)
	HANDLE_COMMAND(SLEEP_NO_TIMER)
	HANDLE_COMMAND(LISTEN_FOR_WAKEUP)
	HANDLE_COMMAND(START_WAKEUP)
	HANDLE_COMMAND(ABORT_WAKEUP)
	HANDLE_COMMAND(WAKEUP_STATE)
	HANDLE_COMMAND(ACTIVE_WAKEUP)
	HANDLE_COMMAND(SET_WAKEUP_SEQUENCE_NUMBER)
	HANDLE_COMMAND(CONFIGURE_EXTERNAL_ROUTE)

	HANDLE_COMMAND(SET_TRANSMIT_HOOK_ACTIVE)

	HANDLE_CALLBACK(RESET_MICRO)
	HANDLE_CALLBACK(STATE)
	HANDLE_CALLBACK(GET_VERSIONS)
	HANDLE_CALLBACK(GET_RIP_ENTRY)
	HANDLE_CALLBACK(INIT)
	HANDLE_CALLBACK(GET_COUNTER)
	HANDLE_CALLBACK(SET_SECURITY_PARAMETERS)
	HANDLE_CALLBACK(SWITCH_TO_NEXT_NETWORK_KEY)
	HANDLE_CALLBACK(FORM_NETWORK)
	HANDLE_CALLBACK(JOIN_NETWORK)
	HANDLE_CALLBACK(RESUME_NETWORK)
	HANDLE_CALLBACK(ATTACH_TO_NETWORK)
	HANDLE_CALLBACK(ENERGY_SCAN)
	HANDLE_CALLBACK(ACTIVE_SCAN)
	HANDLE_CALLBACK(SCAN)
	HANDLE_CALLBACK(SET_ADDRESS)
	HANDLE_CALLBACK(SET_DRIVER_ADDRESS)
	HANDLE_CALLBACK(START_HOST_JOIN_CLIENT)
	HANDLE_CALLBACK(SET_TX_POWER_MODE)
	HANDLE_CALLBACK(GET_TX_POWER_MODE)
	HANDLE_CALLBACK(SET_NETWORK_KEYS)
	HANDLE_CALLBACK(GET_MULTICAST_ENTRY)
	HANDLE_CALLBACK(GET_CCA_THRESHOLD)
	HANDLE_CALLBACK(SET_CCA_THRESHOLD)
	HANDLE_CALLBACK(GET_RADIO_POWER)
	HANDLE_CALLBACK(SET_RADIO_POWER)
	HANDLE_CALLBACK(ECHO)
	HANDLE_CALLBACK(ASSERT_INFO)
	HANDLE_CALLBACK(CONFIGURE_GATEWAY)
	HANDLE_CALLBACK(GET_CHANNEL_CAL_DATA_TOKEN)
	HANDLE_CALLBACK(POLL_FOR_DATA)
	HANDLE_CALLBACK(DEEP_SLEEP)
	HANDLE_CALLBACK(STACK_POLL_FOR_DATA)
	HANDLE_CALLBACK(OK_TO_NAP)
	HANDLE_CALLBACK(DEEP_SLEEP_CALLBACK)
	HANDLE_CALLBACK(RESET_NETWORK_STATE)
	HANDLE_CALLBACK(EXTERNAL_ROUTE_CHANGE)
	HANDLE_CALLBACK(DHCP_SERVER_CHANGE)
	HANDLE_CALLBACK(ADDRESS_CONFIGURATION_CHANGE)
	HANDLE_CALLBACK(REQUEST_DHCP_ADDRESS)
	HANDLE_CALLBACK(COMMISSION_NETWORK)
	HANDLE_CALLBACK(GET_GLOBAL_ADDRESS)
	HANDLE_CALLBACK(GET_DHCP_CLIENT)
	HANDLE_CALLBACK(BECOME_COMMISSIONER)
	HANDLE_CALLBACK(SEND_STEERING_DATA)
	HANDLE_CALLBACK(CHANGE_NODE_TYPE)
	HANDLE_CALLBACK(GET_GLOBAL_PREFIX)
	HANDLE_CALLBACK(RESIGN_GLOBAL_ADDRESS)
	HANDLE_CALLBACK(SLAAC_SERVER_CHANGE)
	HANDLE_CALLBACK(REQUEST_SLAAC_ADDRESS)
	HANDLE_CALLBACK(CUSTOM_NCP_TO_HOST_MESSAGE)
	HANDLE_CALLBACK(NCP_TO_HOST_NO_OP)
	HANDLE_CALLBACK(COMMISSIONER_STATUS)
	HANDLE_CALLBACK(MFGLIB_START_TEST)
	HANDLE_CALLBACK(MFGLIB_RX)
	HANDLE_CALLBACK(MFGLIB_END_TEST)
	HANDLE_CALLBACK(MFGLIB_START)
	HANDLE_CALLBACK(MFGLIB_STOP)
	HANDLE_CALLBACK(MFGLIB_SEND_PACKET_EVENT)
	HANDLE_CALLBACK(MFGLIB_SET)
	HANDLE_CALLBACK(MFGLIB_GET_CHANNEL)
	HANDLE_CALLBACK(MFGLIB_GET_POWER)
	HANDLE_CALLBACK(MFGLIB_GET_POWER_MODE)
	HANDLE_CALLBACK(MFGLIB_GET_SYN_OFFSET)
	HANDLE_CALLBACK(GET_NETWORK_KEY_INFO)
	HANDLE_CALLBACK(GET_NODE_STATUS)
	HANDLE_CALLBACK(ADD_ADDRESS_DATA)
	HANDLE_CALLBACK(CLEAR_ADDRESS_CACHE)
	HANDLE_CALLBACK(LOOKUP_ADDRESS_DATA)
	HANDLE_CALLBACK(PERMIT_JOINING_RETURN)
	HANDLE_CALLBACK(PERMIT_JOINING_HANDLER)
	HANDLE_CALLBACK(GPIO_CURRENT_CONFIG)
	HANDLE_CALLBACK(GPIO_CURRENT_INPUT)
	HANDLE_CALLBACK(GPIO_POWER_CONFIG)
	HANDLE_CALLBACK(WAKEUP)
	HANDLE_CALLBACK(WAKEUP_STATE)
	HANDLE_CALLBACK(LISTEN_FOR_WAKEUP)
	HANDLE_CALLBACK(START_WAKEUP)
	HANDLE_CALLBACK(ABORT_WAKEUP)
	HANDLE_CALLBACK(SLEEP_NO_TIMER)
	HANDLE_CALLBACK(ACTIVE_WAKEUP)
	HANDLE_CALLBACK(SET_WAKEUP_SEQUENCE_NUMBER)
	HANDLE_CALLBACK(CONFIGURE_EXTERNAL_ROUTE)

	default:
		{
			char* temp_cstr = NULL;
			int len = 0;
			if (isalnum(command_identifier>>8)) {
				len = asprintf(&temp_cstr, "UNKNOWN_%c_%d", command_identifier>>8, command_identifier&0xFF);
			} else {
				len = asprintf(&temp_cstr, "UNKNOWN_%04X", command_identifier);
			}
			if (len>0) {
				ret = temp_cstr;
				free(temp_cstr);
			}
		}
		break;
	}
	return ret;
}
