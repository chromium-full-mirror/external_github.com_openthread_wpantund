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
 *      class for NCP stacks running ConnectIP/2.0.0.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include "FirmwareInterface-2.0.0.h"

using namespace nl;
using namespace wpantund;
using namespace EmberCommands;

// ------------------------------------------------------------------------------
// MARK: Constants and Enums
// ------------------------------------------------------------------------------

#include "connectip-2.0/stack/include/error.h"
#include "connectip-2.0/stack/include/counter.h"
#include "connectip-2.0/app/ip-ncp/binary-command-identifiers.h"

enum {
	CB_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER     = CB_GPIO_CURRENT_CONFIG_RETURN_IDENTIFIER,
	CB_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER      = CB_GPIO_CURRENT_INPUT_RETURN_IDENTIFIER,
	CB_GPIO_POWER_CONFIG_COMMAND_IDENTIFIER       = CB_GPIO_POWER_CONFIG_RETURN_IDENTIFIER,

	EMBER_WRITE_NEST_MAC_ADDR_COMMAND_IDENTIFIER  = 0x6990,
	CB_WRITE_NEST_MAC_ADDR_COMMAND_IDENTIFIER     = 0x6390,

	EMBER_NEST_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER = 0x6991,
	CB_NEST_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER    = 0x6391,
};

#define EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG		0x80

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

static const char* counter_cstr[] = { EMBER_COUNTER_STRINGS };

// ------------------------------------------------------------------------------
// MARK: Methods
// ------------------------------------------------------------------------------


boost::shared_ptr<FirmwareInterface>
FirmwareInterface_2_0_0::alloc(int management_version, const std::string& version_string)
{
	return boost::shared_ptr<FirmwareInterface>(new FirmwareInterface_2_0_0(management_version, version_string));
}

FirmwareInterface_2_0_0::FirmwareInterface_2_0_0(int management_version, const std::string& version):
	FirmwareInterface_1_1_3(management_version, version)
{
}

wpantund_status_t
FirmwareInterface_2_0_0::NCPErrorToStatus(uint8_t errorcode)
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
FirmwareInterface_2_0_0::CMD_Form(const WPAN::NetworkId& network, uint8_t node_type, uint32_t channel_mask,const uint8_t *legacy_ula, int8_t tx_power,const uint8_t *ula)
{
	if (GetManagementVersion() <= 8) {
		return FirmwareInterface_1_1_3::CMD_Form(network, node_type, channel_mask, legacy_ula, tx_power,ula);
	}

	char network_id[16];
	uint16_t options = EMBER_NETWORK_ID_OPTION | EMBER_TX_POWER_OPTION;

	// Coverty might complain here with some ominous warning about
	// `network_id` being non-NULL terminated. This is not a bug.
	// `network_id` doesn't need to be NULL-terminated.
	strncpy(network_id, network.name.c_str(), sizeof(network_id));

	if (node_type != 0) {
		options |= EMBER_NODE_TYPE_OPTION;
	}

	if (legacy_ula != NULL) {
		options |= EMBER_LEGACY_ULA_OPTION;
	}

	if (ula != NULL) {
		options |= EMBER_ULA_PREFIX_OPTION;
	}

	return PackArguments(
		EMBER_PACKED_UINT16 "bbusvwb",
		EMBER_FORM_COMMAND_IDENTIFIER,
		sizeof(network_id),
		network_id,
		8,
		ula?ula:(const uint8_t*)"\x00\x00\x00\x00\x00\x00\x00\x00",
		node_type,
		tx_power,
		options,
		channel_mask,
		8,
		legacy_ula?legacy_ula:(const uint8_t*)"\x00\x00\x00\x00\x00\x00\x00\x00"
	);
}

int
FirmwareInterface_2_0_0::Parse_CBState(
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

	if ((legacy_prefix != NULL) && (GetManagementVersion() > 8)) {
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


Data
FirmwareInterface_2_0_0::CMD_HostJoinClientComplete(const uint8_t network_key[16], uint32_t network_key_index, const uint8_t mesh_local_prefix[8])
{
	if(!mesh_local_prefix) {
		static uint8_t default_ula[8] = { 0xfd };
		mesh_local_prefix = default_ula;
	}
	return PackArguments(
		EMBER_PACKED_UINT16 "wbb",
		EMBER_PANA_COMPLETE_COMMAND_IDENTIFIER,
		network_key_index,
		network_key?16:0,
		network_key,
		8,
		mesh_local_prefix
	);
}

Data
FirmwareInterface_2_0_0::CMD_ConfigGateway(uint8_t flags, const uint8_t prefix[], size_t prefix_len, uint32_t preferredLifetime, uint32_t validLifetime, uint8_t domain_id)
{
	(void)prefix_len;
	return PackArguments(
		EMBER_PACKED_UINT16 "ubww",
		EMBER_CONFIG_GATEWAY_COMMAND_IDENTIFIER,
		(flags & 0x02) == 0x02,
		(prefix && (prefix_len >= 8))?8:0,
		prefix,
		preferredLifetime,
		validLifetime
	);
}

Data
FirmwareInterface_2_0_0::CMD_RequestDHCPAddress(const uint8_t prefix[], int prefix_len)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "b",
		EMBER_REQUEST_DHCP_ADDRESS_COMMAND_IDENTIFIER,
		prefix_len,
		prefix
	);
}

Data
FirmwareInterface_2_0_0::CMD_DataPoll()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_DATA_POLL_COMMAND_IDENTIFIER
	);
}

Data
FirmwareInterface_2_0_0::CMD_StackDataPoll(uint32_t cms)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "w",
		EMBER_STACK_DATA_POLL_COMMAND_IDENTIFIER,
		cms
	);
}

Data
FirmwareInterface_2_0_0::CMD_OKToNap()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_OK_TO_NAP_COMMAND_IDENTIFIER
	);
}

int FirmwareInterface_2_0_0::GetIDFor_CBDataPoll() { return CB_DATA_POLL_COMMAND_IDENTIFIER; }
int FirmwareInterface_2_0_0::GetIDFor_CBOKToNap() { return CB_OK_TO_NAP_COMMAND_IDENTIFIER; }
int FirmwareInterface_2_0_0::GetIDFor_CBStackDataPoll() { return CB_STACK_DATA_POLL_COMMAND_IDENTIFIER; }

Data
FirmwareInterface_2_0_0::CMD_SetActiveWakeup(bool active_wakeup)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_ACTIVE_WAKEUP_COMMAND_IDENTIFIER,
		(uint8_t)active_wakeup
	);
}

int
FirmwareInterface_2_0_0::GetIDFor_CBSetActiveWakeup()
{
	return CB_ACTIVE_WAKEUP_COMMAND_IDENTIFIER;
}

NodeType
FirmwareInterface_2_0_0::NodeTypeConvertFromEmber(uint8_t node_type)
{
	NodeType ret;

	node_type &= ~EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG;

	switch (node_type) {
	case 6: // Lurker
		ret = LURKER;
		break;
	case 2: // ROUTER
		ret = ROUTER;
		break;
	case 3: // EMBER_END_DEVICE
		ret = END_DEVICE;
		break;
	case 4: // SLEEPY_END_DEVICE
		ret = SLEEPY_END_DEVICE;
		break;
	default:
		ret = UNKNOWN;
		break;
	}
	return ret;
}

uint8_t
FirmwareInterface_2_0_0::NodeTypeConvertToEmber(NodeType node_type, bool support_legacy)
{
	uint8_t ncp_node_type;

	switch (node_type) {
	case LURKER:
		ncp_node_type = 6; // Lurker
		ncp_node_type |= EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG;
		break;

	case END_DEVICE:
		ncp_node_type = 3; // End Device
		if (support_legacy) {
			ncp_node_type |= EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG;
		}
		break;

	case SLEEPY_END_DEVICE:
		ncp_node_type = 4; // Sleepy End Device
		break;

	default:
	case ROUTER:
		ncp_node_type = 2; // Router
		if (support_legacy) {
			ncp_node_type |= EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG;
		}
		break;
	}
	return ncp_node_type;
}

Data
FirmwareInterface_2_0_0::CMD_WriteNestMACAddr(const uint8_t mac_addr[8])
{
	return PackArguments(
		EMBER_PACKED_UINT16 "b",
		EMBER_WRITE_NEST_MAC_ADDR_COMMAND_IDENTIFIER,
		mac_addr?8:0,
		mac_addr
	);
}

int
FirmwareInterface_2_0_0::GetIDFor_CBWriteNestMACAddr()
{
	return CB_WRITE_NEST_MAC_ADDR_COMMAND_IDENTIFIER;
}

Data
FirmwareInterface_2_0_0::CMD_SetTransmitHookActive(bool using_transmit_hook)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "u",
		EMBER_NEST_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER,
		(uint8_t)using_transmit_hook
	);
}

int
FirmwareInterface_2_0_0::GetIDFor_CBSetTransmitHookActive()
{
	return CB_NEST_SET_TRANSMIT_HOOK_ACTIVE_COMMAND_IDENTIFIER;
}

bool
FirmwareInterface_2_0_0::IsSupported_CBRebootNetwork()
{
	return true;
}

int
FirmwareInterface_2_0_0::GetIDFor_CBRebootNetwork()
{
	return CB_REBOOT_NETWORK_COMMAND_IDENTIFIER;
}

Data
FirmwareInterface_2_0_0::CMD_JoinCommissioned(int8_t tx_power, uint8_t node_type)
{
	return PackArguments(
		EMBER_PACKED_UINT16 "su",
		EMBER_JOIN_COMMISSIONED_COMMAND_IDENTIFIER,
		tx_power,
		node_type
	);
}

bool
FirmwareInterface_2_0_0::IsSupported_CMDCommissionNetwork()
{
	return true;
}

Data
FirmwareInterface_2_0_0::CMD_CommissionNetwork(
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

	return PackArguments(
		EMBER_PACKED_UINT16 "uwbbbb",
		EMBER_COMMISSION_NETWORK_COMMAND_IDENTIFIER,
		perf_chan,
		channel_mask,
		sizeof(network_id)-1, /* Must be -1 because of a bug in ip-modem-app */
		network_id,
		ula?8:0, ula,
		sizeof(network.xpanid), network.xpanid,
		network_key?16:0, network_key
	);
}

int
FirmwareInterface_2_0_0::GetIDFor_CBCommissionNetwork()
{
	return CB_COMMISSION_NETWORK_COMMAND_IDENTIFIER;
}

int
FirmwareInterface_2_0_0::GetReturnStatusFromNCPFrame(const uint8_t* data, size_t len)
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
		case CB_GET_CHANNEL_CAL_TOKEN_COMMAND_IDENTIFIER:
		case CB_GPIO_CURRENT_CONFIG_COMMAND_IDENTIFIER:
		case CB_GPIO_CURRENT_INPUT_COMMAND_IDENTIFIER:
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

int
FirmwareInterface_2_0_0::GetIDFor_CBExternalRouteChange()
{
	return CB_EXTERNAL_ROUTE_CHANGE_COMMAND_IDENTIFIER;
}

int
FirmwareInterface_2_0_0::Parse_CBExternalRouteChange(const Data& data,
	struct in6_addr *prefix,
	uint8_t *prefixLength,
	bool *available)
{
	int ret = -1;
	int prefix_size = 0;
	const uint8_t* prefix_data = NULL;
	uint8_t is_available;

	ret = EmberCommands::UnpackArguments(
			data.data(),
			data.size(),
			EMBER_PACKED_UINT16 "bu",
			NULL, // Skip over the command id
			&prefix_size,
			&prefix_data,
			&is_available
	);

	require(ret != -1, bail);

	// Verify that the size of the address is 16 bytes
	require_action(prefix_size <= sizeof(struct in6_addr), bail, ret = -1);

	require_action(prefix_data != NULL, bail, ret = -1);

	if (prefix != NULL) {
		memset(prefix, 0, sizeof(struct in6_addr));
		memcpy(prefix, prefix_data, prefix_size);
	}

	*prefixLength = prefix_size;
	*available = static_cast<bool>(is_available);

	ret = 0;

bail:
	return ret;

}

int
FirmwareInterface_2_0_0::GetIDFor_CBDHCPServerChange()
{
	return CB_DHCP_SERVER_CHANGE_COMMAND_IDENTIFIER;
}

int
FirmwareInterface_2_0_0::Parse_CBDHCPServerChange(const Data& data, const uint8_t **prefix, int *prefix_len, uint8_t *available)
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
FirmwareInterface_2_0_0::GetIDFor_CBAddressConfiguration()
{
	return CB_ADDRESS_CONFIGURATION_CHANGE_COMMAND_IDENTIFIER;
}

int
FirmwareInterface_2_0_0::Parse_CBAddressConfiguration(const Data& data,
	struct in6_addr *addr,
	uint32_t *validLifetime,
	uint32_t *prefferedLifetime)
{
	int ret = -1;
	int address_size = 0;
	const uint8_t* address_data = NULL;
	uint32_t validLife;
	uint32_t prefferedLife;

	ret = EmberCommands::UnpackArguments(
			data.data(),
			data.size(),
			EMBER_PACKED_UINT16 "bww",
			NULL, // Skip over the command id
			&address_size,
			&address_data,
			&validLife,
			&prefferedLife
	);

	require(ret != -1, bail);

	// Verify that the size of the address is 16 bytes
	require_action(address_size == sizeof(struct in6_addr), bail, ret = -1);

	require_action(address_data != NULL, bail, ret = -1);

	if (addr != NULL) {
		memcpy(addr, address_data, sizeof(struct in6_addr));
	}
	*validLifetime = validLife;
	*prefferedLifetime = prefferedLife;

	ret = 0;

bail:
	return ret;
}

int
FirmwareInterface_2_0_0::GetIDFor_CBRequestDHCPAddress()
{
	return CB_REQUEST_DHCP_ADDRESS_COMMAND_IDENTIFIER;
}

int
FirmwareInterface_2_0_0::Parse_CBRequestDHCPAddress(const Data& data, struct in6_addr *addr)
{
	int ret = -1;
	uint8_t status;
	int address_size = 0;
	const uint8_t* address_data = NULL;

	ret = EmberCommands::UnpackArguments(
			data.data(),
			data.size(),
			EMBER_PACKED_UINT16 "ub",
			NULL, // Skip over the command id
			&status,
			&address_size,
			&address_data
	);

	require(ret != -1, bail);

	// Verify that the size of the address is 16 bytes
	require_action(address_size == sizeof(struct in6_addr), bail, ret = -1);

	require_action(address_data != NULL, bail, ret = -1);

	if (addr != NULL) {
		memcpy(addr, address_data, sizeof(struct in6_addr));
	}

	ret = 0;

bail:
	return ret;
}

int
FirmwareInterface_2_0_0::GetIDFor_CBSetNetworkKeys()
{
	return CB_SET_NETWORK_KEYS_COMMAND_IDENTIFIER;
}

int
FirmwareInterface_2_0_0::Parse_CBSetNetworkKeys(const Data& data, uint32_t *key_index, Data* key)
{
	int ret = -1;
	uint32_t sequence = 0;
	int key_length = 0;
	const uint8_t* key_start = NULL;

	require(
		EmberCommands::UnpackArguments(
			data,
			EMBER_PACKED_UINT16 "wb",
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
FirmwareInterface_2_0_0::Workaround_WaitForCBListenAfterSleepNoTimer(void)
{
	return false;
}

std::string
FirmwareInterface_2_0_0::CounterIndexToString(int x)
{
	std::string ret;

	if (x < 0) {
		goto bail;
	}

	if (x >= FirmwareInterface_2_0_0::GetCounterCount()) {
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
FirmwareInterface_2_0_0::GetCounterCount()
{
	// minus 1 because the list is NULL-terminated.
	return sizeof(counter_cstr)/sizeof(counter_cstr[0])-1;
}

int FirmwareInterface_2_0_0::GetIDFor_CBConfigGatewayReturn() { return CB_CONFIG_GATEWAY_RETURN_COMMAND_IDENTIFIER; }

std::string
FirmwareInterface_2_0_0::CommandIdentifierToString(uint16_t command_identifier)
{
	std::string ret;

#define HANDLE_COMMAND(x) case EMBER_##x##_COMMAND_IDENTIFIER: ret = #x "_COMMAND"; break;
#define HANDLE_CALLBACK(x) case CB_##x##_COMMAND_IDENTIFIER: ret = "CB_" #x; break;

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
	HANDLE_COMMAND(GET_NETWORK_KEY_INFO)
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

	HANDLE_CALLBACK(GPIO_CURRENT_CONFIG)
	HANDLE_CALLBACK(GPIO_CURRENT_INPUT)
	HANDLE_CALLBACK(GPIO_POWER_CONFIG)

	HANDLE_COMMAND(DATA_POLL)
	HANDLE_COMMAND(STACK_DATA_POLL)
	HANDLE_COMMAND(CONFIG_GATEWAY)


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
	HANDLE_CALLBACK(REBOOT_NETWORK)

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

	HANDLE_CALLBACK(DATA_POLL)
	HANDLE_CALLBACK(STACK_DATA_POLL)
	HANDLE_CALLBACK(CONFIG_GATEWAY_RETURN)

	HANDLE_CALLBACK(EXTERNAL_ROUTE_CHANGE)
	HANDLE_CALLBACK(DHCP_SERVER_CHANGE)
	HANDLE_CALLBACK(ADDRESS_CONFIGURATION_CHANGE)
	HANDLE_CALLBACK(REQUEST_DHCP_ADDRESS)

	HANDLE_COMMAND(ECHO)
	HANDLE_COMMAND(ACTIVE_WAKEUP)
	HANDLE_CALLBACK(ECHO)
	HANDLE_CALLBACK(ASSERT_INFO)
	HANDLE_CALLBACK(SLEEP_NO_TIMER)
	HANDLE_CALLBACK(ACTIVE_WAKEUP)

	HANDLE_COMMAND(JOIN_COMMISSIONED)
	HANDLE_COMMAND(COMMISSION_NETWORK)
	HANDLE_CALLBACK(COMMISSION_NETWORK)
	HANDLE_COMMAND(REQUEST_DHCP_ADDRESS)

	// Nest-specific
	HANDLE_COMMAND(NEST_SET_TRANSMIT_HOOK_ACTIVE);
	HANDLE_CALLBACK(NEST_SET_TRANSMIT_HOOK_ACTIVE);
	HANDLE_COMMAND(WRITE_NEST_MAC_ADDR);
	HANDLE_CALLBACK(WRITE_NEST_MAC_ADDR);


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
