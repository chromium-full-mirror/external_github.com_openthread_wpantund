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
 *      class for NCP stacks running ConnectIP/1.1.3.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include "FirmwareInterface-1.1.3.h"
#include <errno.h>

using namespace nl;
using namespace wpantund;
using namespace EmberCommands;

// ------------------------------------------------------------------------------
// MARK: Constants and Enums
// ------------------------------------------------------------------------------

#include "connectip-1.1/stack/include/counter.h"
#include "connectip-1.1/stack/include/error.h"
#include "connectip-1.1/app/ip-ncp/binary-command-identifiers.h"

enum {
	CB_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER     = CB_GPIO_CURRENT_CONFIG_RETURN_IDENTIFIER,
	CB_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER      = CB_GPIO_CURRENT_INPUT_RETURN_IDENTIFIER,
	CB_GPIO_POWER_CONFIG_COMMAND_IDENTIFIER       = CB_GPIO_POWER_CONFIG_RETURN_IDENTIFIER,

	EMBER_GET_CHANNEL_CAL_TOKEN_COMMAND_IDENTIFIER         = 0x6945,
	CB_GET_CHANNEL_CAL_TOKEN_COMMAND_IDENTIFIER            = 0x6341,

	EMBER_NEST_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER = 0x6966,
	CB_NEST_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER    = 0x6366,
};

#define BIT(x)                        (1 << (x))

#define EMBER_NETWORK_ID_OPTION       BIT(0)
#define EMBER_ULA_PREFIX_OPTION       BIT(1)
#define EMBER_EXTENDED_PAN_ID_OPTION  BIT(2)
#define EMBER_PAN_ID_OPTION           BIT(3)
#define EMBER_NODE_TYPE_OPTION        BIT(4)
#define EMBER_TX_POWER_OPTION         BIT(5)
#define EMBER_MASTER_KEY_OPTION       BIT(6)
#define EMBER_LEGACY_ULA_OPTION       BIT(7)

#define EMBER_NETWORK_KEY_OPTION      BIT(0)

static const char* counter_cstr[] = { EMBER_COUNTER_STRINGS };

// ------------------------------------------------------------------------------
// MARK: Methods
// ------------------------------------------------------------------------------

boost::shared_ptr<FirmwareInterface>
FirmwareInterface_1_1_3::alloc(int management_version, const std::string& version_string)
{
	return boost::shared_ptr<FirmwareInterface>(new FirmwareInterface_1_1_3(management_version, version_string));
}

FirmwareInterface_1_1_3::FirmwareInterface_1_1_3(int management_version, const std::string& version):
	FirmwareInterface_Base(management_version, version)
{
}

Data
FirmwareInterface_1_1_3::CMD_GetState()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_STATE_COMMAND_IDENTIFIER
	);
}

wpantund_status_t
FirmwareInterface_1_1_3::NCPErrorToStatus(uint8_t errorcode)
{
	switch(errorcode) {
	case EMBER_INVALID_CALL: return kWPANTUNDStatus_InvalidForCurrentState;
	case EMBER_JOIN_FAILED_SCANNING: return kWPANTUNDStatus_JoinFailedAtScan;
	case EMBER_JOIN_FAILED_MLE2: return kWPANTUNDStatus_JoinFailedAtAuthenticate;
	default: break;
	}

	return errorcode != 0
		? WPANTUND_NCPERROR_TO_STATUS(errorcode)
		: kWPANTUNDStatus_Ok;
}


Data
FirmwareInterface_1_1_3::CMD_Join(const WPAN::NetworkInstance& network, uint8_t node_type, int tx_power)
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
		EMBER_PACKED_UINT16 "bbvusvw",
		EMBER_JOIN_COMMAND_IDENTIFIER,
		sizeof(network_id),
		network_id,
		sizeof(network.xpanid),
		network.xpanid,
		network.panid,
		node_type,
		tx_power,
		options,
		channel_mask
	);
}

Data
FirmwareInterface_1_1_3::CMD_Form(const WPAN::NetworkId& network, uint8_t node_type,uint32_t channel_mask,const uint8_t *ula, int8_t tx_power,const uint8_t *xxx_unused_xxx)
{
	char network_id[16];
	uint16_t options = EMBER_NETWORK_ID_OPTION | EMBER_TX_POWER_OPTION;

	// Coverty might complain here with some ominous warning about
	// `network_id` being non-NULL terminated. This is not a bug.
	// `network_id` doesn't need to be NULL-terminated.
	strncpy(network_id, network.name.c_str(), sizeof(network_id));

	if (node_type != 0) {
		options |= EMBER_NODE_TYPE_OPTION;
	}

	if (ula != NULL) {
		options |= EMBER_ULA_PREFIX_OPTION;
	}

	return PackArguments(
		EMBER_PACKED_UINT16 "bbusvw",
		EMBER_FORM_COMMAND_IDENTIFIER,
		sizeof(network_id),
		network_id,
		8,
		ula?ula:(const uint8_t*)"\x00\x00\x00\x00\x00\x00\x00\x00",
		node_type,
		tx_power,
		options,
		channel_mask
	);
}

int
FirmwareInterface_1_1_3::Parse_CBState(
	const Data& data,
	WPAN::NetworkInstance* network,
	int8_t* tx_power,
	uint8_t* raw_type,
	uint8_t* association_state,
	uint8_t hwaddr[8],
	uint8_t mesh_local_prefix[8],
	uint8_t legacy_prefix[8]
) {
	int ret;
	int network_id_size = 0;
	const char* network_id_data = NULL;
	int mesh_local_prefix_size = 0;
	const uint8_t* mesh_local_prefix_data = NULL;
	int xpanid_size = 0;
	const uint8_t* xpanid_data = NULL;
	uint16_t panid = 0xFFFF;
	uint8_t channel = 0;
	uint8_t node_type = 0;
	int hwaddr_size = 0;
	const uint8_t* hwaddr_data = NULL;
	std::string network_name_string;
	WPAN::NetworkInstance new_network_instance;

	ret = EmberCommands::UnpackArguments(
		data,
		EMBER_PACKED_UINT16 "bbbvuusub",
		NULL, // Skip command identifier
		&network_id_size,
		&network_id_data,
		&mesh_local_prefix_size,
		&mesh_local_prefix_data,
		&xpanid_size,
		&xpanid_data,
		&panid,
		&channel,
		&node_type,
		tx_power,
		association_state,
		&hwaddr_size,
		&hwaddr_data
	);

	require(ret != -1, bail);

	if (mesh_local_prefix_size > 0) {
		require_action(mesh_local_prefix_size == 8, bail, ret = -1);
		if (mesh_local_prefix != NULL) {
			memcpy(mesh_local_prefix, mesh_local_prefix_data, 8);
		}
		if (legacy_prefix != NULL) {
			memcpy(legacy_prefix, mesh_local_prefix_data, 8);
		}
	}

	if ((hwaddr_size > 0) && (mesh_local_prefix != NULL)) {
		require_action(hwaddr_size == 8, bail, ret = -1);
		memcpyrev(hwaddr, hwaddr_data, 8);
	}

	network_name_string = std::string(network_id_data, network_id_data+network_id_size);

	new_network_instance = WPAN::NetworkInstance(
		network_name_string,
		xpanid_data,
		panid,
		channel
	);

	if (raw_type) {
		*raw_type = node_type;
	}

	new_network_instance.type = NodeTypeConvertFromEmber(node_type);

	if (network != NULL) {
		*network = new_network_instance;
	}

bail:
	return ret;
}


Data
FirmwareInterface_1_1_3::CMD_GetRIPEntry(uint8_t index)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_GET_RIP_ENTRY_COMMAND_IDENTIFIER,
		index
	);
}

Data
FirmwareInterface_1_1_3::CMD_GetCCAThreshold()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_GET_CCA_THRESHOLD_COMMAND_IDENTIFIER
	);
}

Data
FirmwareInterface_1_1_3::CMD_SetCCAThreshold(int8_t threshold)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "s",
		EMBER_SET_CCA_THRESHOLD_COMMAND_IDENTIFIER,
		threshold
	);
}

Data
FirmwareInterface_1_1_3::CMD_SetRadioPower(int8_t power_in_dbm)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "s",
		EMBER_SET_RADIO_POWER_COMMAND_IDENTIFIER,
		power_in_dbm
	);
}

Data
FirmwareInterface_1_1_3::CMD_GetTXPowerMode()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_GET_TX_POWER_MODE_COMMAND_IDENTIFIER
	);
}

Data
FirmwareInterface_1_1_3::CMD_SetTXPowerMode(int16_t txPowerMode)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "v",
		EMBER_SET_TX_POWER_MODE_COMMAND_IDENTIFIER,
		txPowerMode
	);
}

Data
FirmwareInterface_1_1_3::CMD_SetPassthruPort(in_port_t port)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "v",
		EMBER_SET_PASSTHROUGH_PORT_COMMAND_IDENTIFIER,
		ntohs(port)
	);
}

Data
FirmwareInterface_1_1_3::CMD_GetCounter(uint8_t type)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_GET_COUNTER_COMMAND_IDENTIFIER,
		type
	);
}

Data
FirmwareInterface_1_1_3::CMD_SetSecurityParams(const uint8_t network_key[16])
{
	return PackArguments(
		EMBER_PACKED_UINT16 "bbv",
		EMBER_SET_SECURITY_PARAMS_COMMAND_IDENTIFIER,
		network_key?16:0,
		network_key,
		0,
		NULL,
		EMBER_NETWORK_KEY_OPTION
	);
}

Data
FirmwareInterface_1_1_3::CMD_HostJoinClientComplete(const uint8_t network_key[16], uint32_t network_key_index, const uint8_t mesh_local_prefix[8])
{
	return PackArguments(
		EMBER_PACKED_UINT16 "ub",
		EMBER_PANA_COMPLETE_COMMAND_IDENTIFIER,
		network_key_index,
		network_key?16:0,
		network_key
	);
}

Data
FirmwareInterface_1_1_3::CMD_Leave()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_REBOOT_NETWORK_COMMAND_IDENTIFIER
	);
}

Data
FirmwareInterface_1_1_3::CMD_Resume()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_RESUME_COMMAND_IDENTIFIER
	);
}

Data
FirmwareInterface_1_1_3::CMD_Scan(int period, uint32_t channel_mask)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "wu",
		EMBER_ACTIVE_SCAN_COMMAND_IDENTIFIER,
		channel_mask,
		period
	);
}

Data
FirmwareInterface_1_1_3::CMD_StopScan()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_STOP_SCAN_COMMAND_IDENTIFIER
	);
}

Data
FirmwareInterface_1_1_3::CMD_PermitJoin(int seconds, bool network_wide)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "vu",
		EMBER_PERMIT_JOIN_COMMAND_IDENTIFIER,
		seconds,
		network_wide
	);
}

Data
FirmwareInterface_1_1_3::CMD_SleepNoTimer()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_SLEEP_NO_TIMER_COMMAND_IDENTIFIER
	);
}

Data
FirmwareInterface_1_1_3::CMD_Listen(bool should_sleep)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_LISTEN_COMMAND_IDENTIFIER,
		should_sleep
	);
}

Data
FirmwareInterface_1_1_3::CMD_StartWakeup(uint8_t data)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_START_WAKEUP_COMMAND_IDENTIFIER,
		data
	);
}

Data
FirmwareInterface_1_1_3::CMD_SetActiveWakeup(bool active_wakeup)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_ACTIVE_WAKEUP_COMMAND_IDENTIFIER,
		(uint8_t)active_wakeup
	);
}

int
FirmwareInterface_1_1_3::GetIDFor_CBSetActiveWakeup()
{
	return CB_ACTIVE_WAKEUP_COMMAND_IDENTIFIER;
}

Data
FirmwareInterface_1_1_3::CMD_Echo(const Data& payload)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "b",
		EMBER_ECHO_COMMAND_IDENTIFIER,
		payload.size(),
		payload.data()
	);
}

Data
FirmwareInterface_1_1_3::CMD_MfgStartTest(int8_t request_rx_callback)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_MFG_START_TEST_COMMAND_IDENTIFIER,
		request_rx_callback
	);
}

Data
FirmwareInterface_1_1_3::CMD_MfgEndTest()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_MFG_END_TEST_COMMAND_IDENTIFIER
	);
}

int
FirmwareInterface_1_1_3::Parse_CBEndTest(const Data& data, uint32_t* rx_packet_count)
{

	if (EmberCommands::UnpackArguments(
		data.data() + 2,
		data.size() - 2,
		"uw",
		NULL,
		rx_packet_count
	) == -1) {
		return -errno;
	}

	return 0;
}

Data
FirmwareInterface_1_1_3::CMD_MfgStart(uint8_t type)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_MFG_START_COMMAND_IDENTIFIER,
		type
	);
}

Data
FirmwareInterface_1_1_3::CMD_MfgStop(uint8_t type)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_MFG_STOP_COMMAND_IDENTIFIER,
		type
	);
}

Data
FirmwareInterface_1_1_3::CMD_MfgTXPacket(const Data& packet, int16_t repeat)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "bv",
		EMBER_MFG_SEND_PACKET_COMMAND_IDENTIFIER,
		(unsigned int)packet.size(),
		packet.data(),
		repeat
	);
}

Data
FirmwareInterface_1_1_3::CMD_MfgSetChannel(int8_t channel)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "uvs",
		EMBER_MFG_SET_COMMAND_IDENTIFIER,
		CHANNEL,
		channel,
		0
	);
}

Data
FirmwareInterface_1_1_3::CMD_MfgSetSYNOffset(int8_t syn_offset)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "uvs",
		EMBER_MFG_SET_COMMAND_IDENTIFIER,
		SYN_OFFSET,
		0,
		syn_offset
	);
}

Data
FirmwareInterface_1_1_3::CMD_MfgSetPower(int16_t power_mode, int8_t power)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "uvs",
		EMBER_MFG_SET_COMMAND_IDENTIFIER,
		POWER,
		power_mode,
		power
	);
}

Data
FirmwareInterface_1_1_3::CMD_MfgGet(uint8_t type)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_MFG_GET_COMMAND_IDENTIFIER,
		type
	);
}

Data
FirmwareInterface_1_1_3::CMD_MfgClockMon(bool enable, uint32_t timerId)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "uw",
		EMBER_SET_NEST_CLOCK_MON_COMMAND_IDENTIFIER,
		static_cast<uint8_t>(enable),
		timerId
	);
}

Data
FirmwareInterface_1_1_3::CMD_GPIOSet(uint8_t port_pin, uint8_t config, uint8_t value)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "uuu",
		EMBER_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER,
		port_pin,
		config,
		value
	);
}

Data
FirmwareInterface_1_1_3::CMD_GPIOGet(uint8_t port_pin)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER,
		port_pin
	);
}

Data
FirmwareInterface_1_1_3::CMD_ChannelCal(uint8_t channel, uint32_t duration)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "uw",
		EMBER_MFG_RUN_MOD_DAC_COMMAND_IDENTIFIER,
		channel,
		duration
	);
}

Data
FirmwareInterface_1_1_3::CMD_ChannelCalGet(uint8_t channel)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_GET_CHANNEL_CAL_TOKEN_COMMAND_IDENTIFIER,
		channel
	);
}



Data
FirmwareInterface_1_1_3::CMD_SetTransmitHookActive(bool using_transmit_hook)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_NEST_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER,
		(uint8_t)using_transmit_hook
	);
}

NodeType
FirmwareInterface_1_1_3::NodeTypeConvertFromEmber(uint8_t node_type)
{
	NodeType ret;

	switch (node_type) {
	case 6: // Lurker
		ret = LURKER;
		break;
	default:
		ret = UNKNOWN;
		break;
	}
	return ret;
}

uint8_t
FirmwareInterface_1_1_3::NodeTypeConvertToEmber(NodeType node_type, bool support_legacy)
{
	uint8_t ncp_node_type;

	switch (node_type) {
	case LURKER:
		ncp_node_type = 6; // Lurker
		break;
	default:
		ncp_node_type = 0; // Unknown Device
		break;
	}
	return ncp_node_type;
}

int
FirmwareInterface_1_1_3::GetIDFor_CBSetAddress()
{
	return CB_SET_ADDRESS_COMMAND_IDENTIFIER;
}

int
FirmwareInterface_1_1_3::Parse_CBSetAddress(const Data& data, struct in6_addr *addr)
{
	int ret = -1;
	int address_size = 0;
	const uint8_t* address_data = NULL;

	ret = EmberCommands::UnpackArguments(
			data.data(),
			data.size(),
			EMBER_PACKED_UINT16 "b",
			NULL, // Skip over the command id
			&address_size,
			&address_data
	);

	require(ret != -1, bail);

	// Verify that the size of the address is 16 bytes
	require(address_size == sizeof(struct in6_addr), bail);

	require(address_data != NULL, bail);

	memcpy(addr, address_data, sizeof(struct in6_addr));

	ret = 0;

bail:
	return ret;
}

int
FirmwareInterface_1_1_3::GetIDFor_CBSetNetworkKeys()
{
	return CB_SET_NETWORK_KEYS_COMMAND_IDENTIFIER;
}

int
FirmwareInterface_1_1_3::Parse_CBSetNetworkKeys(const Data& data, uint32_t *key_index, Data* key)
{
	int ret = -1;
	uint8_t sequence = 0;
	int key_length = 0;
	const uint8_t* key_start = NULL;

	require(
		EmberCommands::UnpackArguments(
			data,
			EMBER_PACKED_UINT16 "ub",
			NULL, // Skip command identifier
			&sequence,
			&key_length,
			&key_start
		) != -1,
		bail
	);

	require(key_length <= 16, bail);

	if (key_index) {
		*key_index = sequence;
	}

	if (key) {
		*key = nl::Data(key_start, key_length);
	}

	ret = 0;

bail:
	return ret;
}

bool
FirmwareInterface_1_1_3::Workaround_WaitForCBListenAfterSleepNoTimer(void)
{
	return true;
}


int FirmwareInterface_1_1_3::GetIDFor_CBMfgEndTest(void) { return CB_MFG_END_TEST_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBActiveScan() { return CB_ACTIVE_SCAN_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBAssertInfo() { return CB_ASSERT_INFO_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBCounters() { return CB_COUNTERS_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBEcho() { return CB_ECHO_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBForm() { return CB_FORM_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBGetCCAThreshold() { return CB_GET_CCA_THRESHOLD_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBGetChannelCalToken() { return CB_GET_CHANNEL_CAL_TOKEN_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBGetTXPowerMode() { return CB_GET_TX_POWER_MODE_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBGetVersions() { return CB_GET_VERSIONS_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBInitHost() { return CB_INIT_HOST_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBJoin() { return CB_JOIN_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBListen() { return CB_LISTEN_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBMfgGetChannel() { return CB_MFG_GET_CHANNEL_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBMfgGetPower() { return CB_MFG_GET_POWER_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBMfgGetPowerMode() { return CB_MFG_GET_POWER_MODE_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBMfgGetSynOffset() { return CB_MFG_GET_SYN_OFFSET_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBMfgRX() { return CB_MFG_RX_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBMfgSendPacket() { return CB_MFG_SEND_PACKET_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBMfgSet() { return CB_MFG_SET_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBMfgStart() { return CB_MFG_START_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBMfgStartTest() { return CB_MFG_START_TEST_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBMfgStop() { return CB_MFG_STOP_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBPermitJoin() { return CB_PERMIT_JOIN_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBPermitJoinReturn() { return CB_PERMIT_JOIN_RETURN_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBReset() { return CB_RESET_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBResume() { return CB_RESUME_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBRIPEntry() { return CB_RIP_ENTRY_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBScanReturn() { return CB_SCAN_RETURN_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBSetCCAThreshold() { return CB_SET_CCA_THRESHOLD_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBSetRadioPower() { return CB_SET_RADIO_POWER_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBSetSecurityParams() { return CB_SET_SECURITY_PARAMS_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBSetTXPowerMode() { return CB_SET_TX_POWER_MODE_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBSleepNoTimer() { return CB_SLEEP_NO_TIMER_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBStartHostJoinClient() { return CB_START_PANA_CLIENT_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBStartWakeup() { return CB_START_WAKEUP_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBState() { return CB_STATE_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBWakeup() { return CB_WAKEUP_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBGPIOCurrentConfig() { return CB_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBGPIOCurrentInput() { return CB_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER; }
int FirmwareInterface_1_1_3::GetIDFor_CBSetTransmitHookActive() { return CB_NEST_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER; }

std::string
FirmwareInterface_1_1_3::CounterIndexToString(int x)
{
	std::string ret;

	if (x < 0) {
		goto bail;
	}

	if (x >= FirmwareInterface_1_1_3::GetCounterCount()) {
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
FirmwareInterface_1_1_3::GetCounterCount()
{
	// minus 1 because the list is NULL-terminated.
	return sizeof(counter_cstr)/sizeof(counter_cstr[0])-1;
}

Data
FirmwareInterface_1_1_3::CMD_FFWakeup(void)
{
	return PackArguments(
		EMBER_PACKED_UINT16,
		EMBER_FF_WAKEUP_COMMAND_IDENTIFIER
	);
}


std::string
FirmwareInterface_1_1_3::CommandIdentifierToString(uint16_t command_identifier)
{
	std::string ret;

#define HANDLE_COMMAND(x) case EMBER_ ## x ## _COMMAND_IDENTIFIER : ret = #x "_COMMAND"; break;
#define HANDLE_CALLBACK(x) case CB_ ## x ## _COMMAND_IDENTIFIER : ret = "CB_" #x; break;

	switch(command_identifier) {

	// host -> ip-modem-app commands
	HANDLE_COMMAND(REBOOT)
	HANDLE_COMMAND(REBOOT_NETWORK)
	HANDLE_COMMAND(INIT_HOST)
	HANDLE_COMMAND(STATE)
	HANDLE_COMMAND(GET_VERSIONS)
	HANDLE_COMMAND(FORM)
	HANDLE_COMMAND(JOIN)
	HANDLE_COMMAND(RESUME)
	HANDLE_COMMAND(PANA_COMPLETE)
	HANDLE_COMMAND(SET_SECURITY_PARAMS)
	HANDLE_COMMAND(PERMIT_JOIN)
	HANDLE_COMMAND(ENERGY_SCAN)
	HANDLE_COMMAND(ACTIVE_SCAN)
	HANDLE_COMMAND(STOP_SCAN)
	HANDLE_COMMAND(GET_RIP_ENTRY)
	HANDLE_COMMAND(GET_COUNTER)
	HANDLE_COMMAND(CLEAR_COUNTERS)
	HANDLE_COMMAND(SET_TX_POWER_MODE)
	HANDLE_COMMAND(GET_TX_POWER_MODE)
	HANDLE_COMMAND(SLEEP_NO_TIMER)
	HANDLE_COMMAND(LISTEN)
	HANDLE_COMMAND(START_WAKEUP)
	HANDLE_COMMAND(END_WAKEUP)
	HANDLE_COMMAND(CONFIG_WAKEUP)
	HANDLE_COMMAND(WAKEUP_STATE)
	HANDLE_COMMAND(FF_WAKEUP)
	HANDLE_COMMAND(GET_CCA_THRESHOLD)
	HANDLE_COMMAND(SET_CCA_THRESHOLD)
	HANDLE_COMMAND(GET_RADIO_POWER)
	HANDLE_COMMAND(SET_RADIO_POWER)

	HANDLE_COMMAND(SET_PASSTHROUGH_PORT)
	HANDLE_COMMAND(SET_NEST_CLOCK_MON)
	HANDLE_COMMAND(GPIO_CURRENT_CONFIG)
	HANDLE_COMMAND(GPIO_CURRENT_INPUT)
	HANDLE_COMMAND(GPIO_POWER_CONFIG)
	HANDLE_COMMAND(GPIO_RADIO_POWER_MASK)

	// mfg commands
	HANDLE_COMMAND(MFG_START_TEST)
	HANDLE_COMMAND(MFG_END_TEST)
	HANDLE_COMMAND(MFG_START)
	HANDLE_COMMAND(MFG_STOP)
	HANDLE_COMMAND(MFG_SEND_PACKET)
	HANDLE_COMMAND(MFG_SET)
	HANDLE_COMMAND(MFG_GET)
	HANDLE_COMMAND(MFG_RUN_MOD_DAC)

	HANDLE_COMMAND(GET_CHANNEL_CAL_TOKEN)
	HANDLE_CALLBACK(GET_CHANNEL_CAL_TOKEN)

	// callbacks (ip-modem-app -> host)
	HANDLE_CALLBACK(RESET)
	HANDLE_CALLBACK(STATE)
	HANDLE_CALLBACK(GET_VERSIONS)
	HANDLE_CALLBACK(RIP_ENTRY)
	HANDLE_CALLBACK(INIT_HOST)
	HANDLE_CALLBACK(COUNTERS)
	HANDLE_CALLBACK(SET_SECURITY_PARAMS)
	HANDLE_CALLBACK(SET_NEXT_NETWORK_KEY)
	HANDLE_CALLBACK(GET_NETWORK_KEY_INFO)
	HANDLE_CALLBACK(FORM)
	HANDLE_CALLBACK(JOIN)
	HANDLE_CALLBACK(RESUME)
	HANDLE_CALLBACK(PERMIT_JOIN_RETURN)
	HANDLE_CALLBACK(PERMIT_JOIN)
	HANDLE_CALLBACK(ENERGY_SCAN)
	HANDLE_CALLBACK(ACTIVE_SCAN)
	HANDLE_CALLBACK(SCAN_RETURN)
	HANDLE_CALLBACK(SET_ADDRESS)
	HANDLE_CALLBACK(SET_DRIVER_ADDRESS)
	HANDLE_CALLBACK(START_PANA_CLIENT)
	HANDLE_CALLBACK(SET_TX_POWER_MODE)
	HANDLE_CALLBACK(GET_TX_POWER_MODE)
	HANDLE_CALLBACK(SET_NETWORK_KEYS)
	HANDLE_CALLBACK(WAKEUP)
	HANDLE_CALLBACK(WAKEUP_STATE)
	HANDLE_CALLBACK(LISTEN)
	HANDLE_CALLBACK(START_WAKEUP)
	HANDLE_CALLBACK(END_WAKEUP)
	HANDLE_CALLBACK(PANA_COMPLETE)
	HANDLE_CALLBACK(GET_CCA_THRESHOLD)
	HANDLE_CALLBACK(SET_CCA_THRESHOLD)
	HANDLE_CALLBACK(GET_RADIO_POWER)
	HANDLE_CALLBACK(SET_RADIO_POWER)

	// mfg callbacks
	HANDLE_CALLBACK(MFG_START_TEST)
	HANDLE_CALLBACK(MFG_RX)
	HANDLE_CALLBACK(MFG_END_TEST)
	HANDLE_CALLBACK(MFG_START)
	HANDLE_CALLBACK(MFG_STOP)
	HANDLE_CALLBACK(MFG_SEND_PACKET)
	HANDLE_CALLBACK(MFG_SET)
	HANDLE_CALLBACK(MFG_GET)
	HANDLE_CALLBACK(MFG_RUN_MOD_DAC)
	HANDLE_CALLBACK(MFG_GET_CHANNEL)
	HANDLE_CALLBACK(MFG_GET_POWER)
	HANDLE_CALLBACK(MFG_GET_POWER_MODE)
	HANDLE_CALLBACK(MFG_GET_SYN_OFFSET)



	// V1.1 and above only.
	HANDLE_COMMAND(ECHO)
	HANDLE_COMMAND(ACTIVE_WAKEUP)
	HANDLE_CALLBACK(ECHO)
	HANDLE_CALLBACK(ASSERT_INFO)
	HANDLE_CALLBACK(SLEEP_NO_TIMER)
	HANDLE_CALLBACK(ACTIVE_WAKEUP)

	HANDLE_COMMAND(NEST_SET_TRANSMIT_HOOK_ACTIVE);

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

int
FirmwareInterface_1_1_3::Parse_CBActiveScan(
	const Data& data,
	WPAN::NetworkInstance* network_instance
) {
	int i = 0;
	uint8_t permit_join = 0;
	uint8_t router_capacity = 0;
	uint8_t child_capacity = 0;
	uint8_t channel = 0;
	uint8_t lqi = 0;
	uint8_t rssi = 0;
	uint16_t parent_id = 0;
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
		"uuuuuuvbvbbu" /* "bu" Leaving out fragment stuff */,
		&permit_join,
		&router_capacity,
		&child_capacity,
		&channel,
		&lqi,
		&rssi,
		&parent_id,
		&network_id_size,
		&network_id_data,
		&panid,
		&xpanid_size,
		&xpanid_data,
		&source_size,
		&source_data,
		&type
	) == -1) {
		return -errno;
	}

	char network_name[network_id_size+1];

	strncpy(network_name, network_id_data, network_id_size);

	// Make sure we zero terminate.
	network_name[network_id_size] = 0;

	// Strip any UTF-8 characters for now.
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

int
FirmwareInterface_1_1_3::GetReturnStatusFromNCPFrame(const uint8_t* data, size_t len)
{
	uint8_t ret = 0;
	if (len > 2) {
		uint16_t command = 0;
		EmberCommands::UnpackArguments(data, len, EMBER_PACKED_UINT16 "u",&command,&ret);

		switch (command) {
		// Commands listed here have no return status in their response,
		// so manually ensure that it is zero.
		case CB_STATE_COMMAND_IDENTIFIER:
		case CB_WAKEUP_COMMAND_IDENTIFIER:
		case CB_GET_CCA_THRESHOLD_COMMAND_IDENTIFIER:
		case CB_MFG_GET_POWER_MODE_COMMAND_IDENTIFIER:
		case CB_GET_TX_POWER_MODE_COMMAND_IDENTIFIER:
		case CB_MFG_GET_CHANNEL_COMMAND_IDENTIFIER:
		case CB_MFG_GET_SYN_OFFSET_COMMAND_IDENTIFIER:
		case CB_MFG_GET_POWER_COMMAND_IDENTIFIER:
		case CB_SET_ADDRESS_COMMAND_IDENTIFIER:
		case CB_COUNTERS_COMMAND_IDENTIFIER:
			ret = 0;
			break;

		case CB_MFG_SET_COMMAND_IDENTIFIER:
		case CB_MFG_START_COMMAND_IDENTIFIER:
		case CB_MFG_STOP_COMMAND_IDENTIFIER:
			// The status byte is in a weird spot for these MFG commands.
			EmberCommands::UnpackArguments(data, len, EMBER_PACKED_UINT16 "uu",&command,NULL,&ret);
			break;

		default:
			break;
		}
	}
	return NCPErrorToStatus(ret);
}
