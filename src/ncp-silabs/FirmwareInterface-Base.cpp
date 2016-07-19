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
 *      This file contains the base class for the FirmwareInterface
 *      class. It implements the bare minimum required commands to be
 *      able to interrogate the NCP to learn version information.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <syslog.h>

#include "FirmwareInterface-Base.h"
#include "FirmwareInterface.h"

#include <boost/algorithm/string/predicate.hpp>

using namespace nl;
using namespace wpantund;
using namespace EmberCommands;

// ------------------------------------------------------------------------------
// MARK: Constants and Enums
// ------------------------------------------------------------------------------

#include "silabs-thread-1.0/app/tmsp/tmsp-enum.h"

typedef uint8_t EmberVersionType;

enum {
  EMBER_VERSION_TYPE_INTERNAL = 0,
  EMBER_VERSION_TYPE_ALPHA    = 1,
  EMBER_VERSION_TYPE_BETA     = 2,
  EMBER_VERSION_TYPE_GA       = 3,
  EMBER_VERSION_TYPE_SPECIAL  = 4,
};

// ------------------------------------------------------------------------------
// MARK: Methods
// ------------------------------------------------------------------------------

boost::shared_ptr<FirmwareInterface>
FirmwareInterface_Base::alloc(int management_version, const std::string& version_string)
{
	return boost::shared_ptr<FirmwareInterface>(new FirmwareInterface_Base(management_version, version_string));
}


FirmwareInterface_Base::FirmwareInterface_Base(int management_version, const std::string& version):
	mVersionString(version),
	mManagementVersion(management_version)
{
	static bool did_show_legal_notice = false;

	if (!did_show_legal_notice) {
		syslog(LOG_WARNING, "THIS PLUGIN IS NOT OPEN SOURCE SOFTWARE.");
		did_show_legal_notice = true;
	}
}

FirmwareInterface_Base::~FirmwareInterface_Base()
{
}

bool
FirmwareInterface_Base::IsSupported_CMDSetActiveWakeup()
{
	return mManagementVersion >= 5;
}

const std::string&
FirmwareInterface_Base::GetVersionString(void)
{
	return mVersionString;
}

int
FirmwareInterface_Base::GetIDFor_CBGetVersions()
{
	return CB_GET_VERSIONS_COMMAND_IDENTIFIER;
}

int
FirmwareInterface_Base::Parse_CBGetVersions(const Data& data, int* ptr_to_mgnt_ver, std::string* ptr_to_ver_str)
{
	char version_cstr[256];
	const char* stack_name_str = NULL;
	int stack_name_len = 0;
	char* stack_info_str = NULL;
	int stack_info_len = 0;
	uint16_t management_version = 0;
	uint16_t packed_stack_version = 0;
	uint16_t stack_build = 0;
	uint8_t version_type = 0;
	const char* version_type_cstr = NULL;
	int ret = 0;

	ret = EmberCommands::UnpackArguments(
		data.data(),
		data.size(),
		EMBER_PACKED_UINT16 "bvvvub",
		NULL,
		&stack_name_len,
		&stack_name_str,
		&management_version,
		&packed_stack_version,
		&stack_build,
		&version_type,
		&stack_info_len,
		&stack_info_str
	);

	if (ret == -1) {
		throw parse_error();
	}

	uint8_t version_major = ((packed_stack_version >> 12) & 0xF);
	uint8_t version_minor = ((packed_stack_version >> 8) & 0xF);
	uint8_t version_patch = ((packed_stack_version >> 4) & 0xF);
	uint8_t version_special = ((packed_stack_version >> 0) & 0xF);

	// Work-around for thread stack naming itself "ConnectIP"
	if ((management_version == 9)
		&& (strncmp("ConnectIP", stack_name_str, stack_name_len) == 0)
		&& (version_major == 1)
	) {
		stack_name_str = "SiLabsThread";
		stack_name_len = (int)strlen(stack_name_str);
	}

	switch (version_type) {
	case EMBER_VERSION_TYPE_INTERNAL:
		version_type_cstr = " (Internal)";
		break;
	case EMBER_VERSION_TYPE_ALPHA:
		version_type_cstr = " (Alpha)";
		break;
	case EMBER_VERSION_TYPE_BETA:
		version_type_cstr = " (Beta)";
		break;
	case EMBER_VERSION_TYPE_SPECIAL:
		version_type_cstr = " (Special)";
		break;
	default:
	case EMBER_VERSION_TYPE_GA:
		version_type_cstr = "";
		break;
	}

	if (management_version > 0xFF) {
		snprintf(
			version_cstr,
			sizeof(version_cstr),
			"%.*s/%d.%d.%db%d s%d m%d.%d%s; %.*s",
			stack_name_len,
			stack_name_str,
			version_major,
			version_minor,
			version_patch,
			stack_build,
			version_special,
			management_version >> 8,
			management_version & 0xFF,
			version_type_cstr,
			stack_info_len,
			stack_info_str
		);
	} else {
		snprintf(
			version_cstr,
			sizeof(version_cstr),
			"%.*s/%d.%d.%db%d s%d m%d%s; %.*s",
			stack_name_len,
			stack_name_str,
			version_major,
			version_minor,
			version_patch,
			stack_build,
			version_special,
			management_version,
			version_type_cstr,
			stack_info_len,
			stack_info_str
		);
	}

	if (ptr_to_ver_str) {
		*ptr_to_ver_str = version_cstr;
	}

	if (ptr_to_mgnt_ver) {
		*ptr_to_mgnt_ver = management_version;
	}

	return ret;
}

int
FirmwareInterface_Base::GetManagementVersion(void)
{
	return mManagementVersion;
}

Data
FirmwareInterface_Base::CMD_GetVersions()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_GET_VERSIONS_COMMAND_IDENTIFIER
	);
}

Data
FirmwareInterface_Base::CMD_InitHost()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_INIT_HOST_COMMAND_IDENTIFIER
	);
}

int
FirmwareInterface_Base::GetIDFor_CBInitHost()
{
	return CB_INIT_COMMAND_IDENTIFIER;
}

Data
FirmwareInterface_Base::CMD_Reset()
{
	return PackArguments(
		EMBER_PACKED_UINT16 "",
		EMBER_RESET_MICRO_COMMAND_IDENTIFIER
	);
}

int
FirmwareInterface_Base::GetIDFor_CBReset()
{
	return CB_RESET_MICRO_COMMAND_IDENTIFIER;
}

wpantund_status_t
FirmwareInterface_Base::NCPErrorToStatus(uint8_t errorcode)
{
	return errorcode != 0
		? WPANTUND_NCPERROR_TO_STATUS(errorcode)
		: kWPANTUNDStatus_Ok;
}

std::string
FirmwareInterface_Base::CommandIdentifierToString(uint16_t command_identifier)
{
	std::string ret;

#define HANDLE_COMMAND(x) case EMBER_##x##_COMMAND_IDENTIFIER: ret = #x "_COMMAND"; break;
#define HANDLE_CALLBACK(x) case CB_##x##_COMMAND_IDENTIFIER: ret = "CB_" #x; break;

	switch(command_identifier) {

	// host -> ip-modem-app commands
	HANDLE_COMMAND(RESET_MICRO)
	HANDLE_COMMAND(INIT_HOST)
	HANDLE_COMMAND(GET_VERSIONS)

	// callbacks (ip-modem-app -> host)
	HANDLE_CALLBACK(RESET_MICRO)
	HANDLE_CALLBACK(GET_VERSIONS)
	HANDLE_CALLBACK(INIT)
	HANDLE_CALLBACK(STATE)

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
FirmwareInterface_Base::GetReturnStatusFromNCPFrame(const uint8_t* data, size_t len)
{
	uint8_t ret = 0;
	if (len > 2) {
		uint16_t command = 0;
		EmberCommands::UnpackArguments(data, len, EMBER_PACKED_UINT16 "u",&command,&ret);

		switch (command) {
		// Commands listed here have no return status in their response,
		// so manually ensure that it is zero.
		case CB_STATE_COMMAND_IDENTIFIER:
			ret = 0;
			break;

		default:
			break;
		}
	}
	return NCPErrorToStatus(ret);
}

std::string
FirmwareInterface_Base::CounterIndexToString_Helper(int x, const char* counter_cstr[])
{
	std::string ret;
	const char* next;

	for(next = counter_cstr[x]; *next != 0; next++) {
		if (*next == ' ' || *next == '-') {
			ret.append(1, '_');
		} else {
			ret.append(1, toupper(*next));
		}
	}

	return ret;
}

std::string
FirmwareInterface_Base::CounterIndexToString(int x)
{
	char temp_cstr[64];
	snprintf(temp_cstr, sizeof(temp_cstr), "Counter-%d", static_cast<uint8_t>(x));
	return temp_cstr;
}

int
FirmwareInterface_Base::StringToCounterIndex(const std::string& key)
{
	int ret = -1;

	if (key.find("Counter-") == 0) {
		ret = static_cast<uint8_t>(strtol(key.c_str() + 8, NULL, 0));
	}

	if (ret < 0) {
		char temp_cstr[64];
		int i;

		temp_cstr[sizeof(temp_cstr)-1] = 0;
		for (i=0; i < GetCounterCount(); i++) {
			char* next_space = NULL;
			strncpy(temp_cstr, CounterIndexToString(i).c_str(), sizeof(temp_cstr)-1);
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

int
FirmwareInterface_Base::GetCounterCount()
{
	return 0;
}

Data
FirmwareInterface_Base::WrapCustomCommand(const Data& x)
{
	return x;
}

Data
FirmwareInterface_Base::UnwrapCustomCallback(const Data& x)
{
	const uint8_t* data(x.data());
	size_t len(x.size());
	UnwrapCustomCallback(data, len);
	return Data(data,len);
}

int
FirmwareInterface_Base::UnwrapCustomCallback(const uint8_t*&data, size_t&len)
{
	(void)data;
	(void)len;
	return 0;
}

int
FirmwareInterface_Base::GetCMDID(const uint8_t* data, size_t len)
{
	if (len >= 2) {
		return (data[0] << 8) + data[1];
	}
	return -1;
}

int
FirmwareInterface_Base::GetCMDID(const Data& x)
{
	return GetCMDID(x.data(), x.size());
}

std::string
FirmwareInterface_Base::DescribeCommandOrCallback(const uint8_t* data, size_t len)
{
	size_t prevlen(len);
	UnwrapCustomCallback(data, len);

	if (len != prevlen) {
		return "WRAPPED(" + CommandIdentifierToString(GetCMDID(data, len)) + ")";
	}

	return CommandIdentifierToString(GetCMDID(data, len));
}




/////////////////////////////////////////////////////////////////////////////
// Everything below this point is boilerplate "not_implemented" functions. //
/////////////////////////////////////////////////////////////////////////////



int
FirmwareInterface_Base::Parse_CBState(
	const Data& data,
	WPAN::NetworkInstance* network,
	int8_t* tx_power,
	uint8_t* raw_type,
	uint8_t* association_state,
	uint8_t hwaddr[8],
	uint8_t mesh_local_prefix[8],
	uint8_t legacy_prefix[8]
) {
	throw not_implemented();
}

int
FirmwareInterface_Base::Parse_CBAddressConfigurationChange(const Data& data, struct in6_addr* address, uint32_t* valid_lifetime, uint32_t* preferred_lifetime, uint8_t* flags)
{
	throw not_implemented();
}


Data
FirmwareInterface_Base::CMD_GetState()
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_Join(const WPAN::NetworkInstance& network, uint8_t node_type, int tx_power)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_Form(const WPAN::NetworkId& network, uint8_t node_type,uint32_t channel_mask,const uint8_t *legacy_ula, int8_t tx_power,const uint8_t *ula)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_GetRIPEntry(uint8_t index)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_GetCCAThreshold()
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_SetCCAThreshold(int8_t threshold)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_SetRadioPower(int8_t power_in_dbm)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_GetTXPowerMode()
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_SetTXPowerMode(int16_t txPowerMode)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_SetPassthruPort(in_port_t port)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_GetCounter(uint8_t type)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_SetSecurityParams(const uint8_t network_key[16])
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_HostJoinClientComplete(const uint8_t network_key[16], uint32_t network_key_index, const uint8_t mesh_local_prefix[8])
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_Leave()
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_Resume()
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_Scan(int period, uint32_t channel_mask)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_EnergyScan(int period, uint32_t channel_mask)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_StopScan()
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_PermitJoin(int seconds, bool network_wide)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_SleepNoTimer()
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_Listen(bool should_sleep)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_StartWakeup(uint8_t data)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_DataPoll()
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_StackDataPoll(uint32_t cms)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_OKToNap()
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_ConfigGateway(uint8_t flags, const uint8_t prefix[], size_t prefix_len, uint32_t preferredLifetime, uint32_t validLifetime, uint8_t domain_id)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_RequestDHCPAddress(const uint8_t prefix[], int prefix_len)
{
	throw not_implemented();
}


Data
FirmwareInterface_Base::CMD_SetActiveWakeup(bool active_wakeup)
{
	throw not_implemented();
}

int
FirmwareInterface_Base::GetIDFor_CBSetActiveWakeup() {
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_Echo(const Data& payload)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_MfgStartTest(int8_t request_rx_callback)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_MfgEndTest()
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_MfgStart(uint8_t type)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_MfgStop(uint8_t type)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_MfgTXPacket(const Data& packet, int16_t repeat)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_MfgSetChannel(int8_t channel)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_MfgSetSYNOffset(int8_t syn_offset)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_MfgSetPower(int16_t power_mode, int8_t power)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_MfgGet(uint8_t type)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_SetTransmitHookActive(bool using_transmit_hook)
{
	throw not_implemented();
}

NodeType
FirmwareInterface_Base::NodeTypeConvertFromEmber(uint8_t node_type)
{
	return UNKNOWN;
}

uint8_t
FirmwareInterface_Base::NodeTypeConvertToEmber(NodeType node_type, bool support_legacy)
{
	return 0;
}

int
FirmwareInterface_Base::Parse_CBActiveScan(
	const Data& data,
	WPAN::NetworkInstance* network_instance
) {
	throw not_implemented();
}

int
FirmwareInterface_Base::Parse_CBEnergyScan(
	const Data& data,
	uint8_t *channel,
	int8_t *maxRssi
) {
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_WriteNestMACAddr(const uint8_t mac_addr[8])
{
	throw not_implemented();
}

int
FirmwareInterface_Base::GetIDFor_CBWriteNestMACAddr()
{
	throw not_implemented();
}

int
FirmwareInterface_Base::GetIDFor_CBSetTransmitHookActive()
{
	throw not_implemented();
}

int
FirmwareInterface_Base::Parse_CBEndTest(const Data& data, uint32_t* rx_packet_count)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_MfgClockMon(bool enable, uint32_t timerId)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_GPIOSet(uint8_t port_pin, uint8_t config, uint8_t value)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_GPIOGet(uint8_t port_pin)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_ChannelCal(uint8_t channel, uint32_t duration)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_ChannelCalGet(uint8_t channel)
{
	throw not_implemented();
}

bool
FirmwareInterface_Base::IsSupported_CBRebootNetwork()
{
	return false;
}

int
FirmwareInterface_Base::GetIDFor_CBRebootNetwork()
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_JoinCommissioned(int8_t tx_power, uint8_t node_type)
{
	throw not_implemented();
}

bool
FirmwareInterface_Base::IsSupported_CMDCommissionNetwork()
{
	return false;
}

Data
FirmwareInterface_Base::CMD_CommissionNetwork(
	const WPAN::NetworkId& network,
	uint8_t perf_chan,
	uint32_t channel_mask,
	const uint8_t ula[8],
	const uint8_t network_key[16]
) {
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_FFWakeup(void)
{
	throw not_implemented(); // EMBER_FF_WAKEUP_COMMAND_IDENTIFIER
}

int
FirmwareInterface_Base::GetIDFor_CBCommissionNetwork()
{
	throw not_implemented();
}

int
FirmwareInterface_Base::GetIDFor_CBExternalRouteChange()
{
	return 0;
}

int
FirmwareInterface_Base::Parse_CBExternalRouteChange(const Data& data,
	struct in6_addr *prefix,
	uint8_t *prefixLength,
	bool *available)
{
	throw not_implemented();
}

int
FirmwareInterface_Base::GetIDFor_CBDHCPServerChange()
{
	return 0;
}

int
FirmwareInterface_Base::Parse_CBDHCPServerChange(const Data& data, const uint8_t **prefix, int *prefix_len, uint8_t *available)
{
	throw not_implemented();
}

int
FirmwareInterface_Base::GetIDFor_CBAddressConfiguration()
{
	return 0;
}

int
FirmwareInterface_Base::Parse_CBAddressConfiguration(const Data& data,
	struct in6_addr *addr,
	uint32_t *validLifetime,
	uint32_t *prefferedLifetime)
{
	throw not_implemented();
}

int
FirmwareInterface_Base::GetIDFor_CBRequestDHCPAddress()
{
	return 0;
}

int
FirmwareInterface_Base::Parse_CBRequestDHCPAddress(const Data& data, struct in6_addr *addr)
{
	throw not_implemented();
}

int
FirmwareInterface_Base::GetIDFor_CBSetAddress()
{
	return 0;
}

int
FirmwareInterface_Base::Parse_CBSetAddress(const Data& data, struct in6_addr *addr)
{
	throw not_implemented();
}

int
FirmwareInterface_Base::GetIDFor_CBSetNetworkKeys()
{
	return 0;
}

int
FirmwareInterface_Base::GetIDFor_CBSetDriverAddress() {
	return 0;
}

int
FirmwareInterface_Base::Parse_CBSetNetworkKeys(const Data& data, uint32_t *key_index, Data* key)
{
	throw not_implemented();
}

bool
FirmwareInterface_Base::Workaround_WaitForCBListenAfterSleepNoTimer(void)
{
	return false;
}

bool
FirmwareInterface_Base::Workaround_CommissionNetworkRequired(void)
{
	return false;
}

int
FirmwareInterface_Base::GetIDFor_CBMfgEndTest(void)
{
	return 0;
}

int FirmwareInterface_Base::GetIDFor_CBActiveScan() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBAssertInfo() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBConfigGatewayReturn() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBCounters() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBDataPoll() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBEcho() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBEnergyScan() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBForm() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBGetCCAThreshold() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBGetChannelCalToken() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBGetTXPowerMode() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBJoin() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBListen() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBMfgGetChannel() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBMfgGetPower() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBMfgGetPowerMode() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBMfgGetSynOffset() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBMfgRX() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBMfgSendPacket() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBMfgSet() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBMfgStart() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBMfgStartTest() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBMfgStop() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBOKToNap() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBPermitJoin() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBPermitJoinReturn() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBResume() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBRIPEntry() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBScanReturn() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBSetCCAThreshold() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBSetRadioPower() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBSetSecurityParams() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBSetTXPowerMode() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBSleepNoTimer() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBStackDataPoll() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBStartHostJoinClient() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBStartWakeup() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBState() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBWakeup() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBGPIOCurrentConfig() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBGPIOCurrentInput() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBCustomNCPToHostMessage() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBGetGlobalAddress() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBAddressConfigurationChange() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBResignGlobalAddress() { return 0; }

Data
FirmwareInterface_Base::CMD_ResignGlobalAddress(const struct in6_addr* address)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_GetGlobalAddresses(const uint8_t prefix[], int prefix_len)
{
	throw not_implemented();
}

int
FirmwareInterface_Base::GetMinTXPower()
{
	return -43;
}

int
FirmwareInterface_Base::GetMaxTXPower()
{
	return 8;
}

int
FirmwareInterface_Base::Parse_CBGetGlobalAddress(const Data& data, struct in6_addr* address, uint32_t* preferred_lifetime,
		uint32_t* valid_lifetime, uint8_t* flags)
{
	throw not_implemented();
}

int FirmwareInterface_Base::GetIDFor_CBChangeNodeType() { return 0; }

Data
FirmwareInterface_Base::CMD_ChangeNodeType(uint8_t type)
{
	throw not_implemented();
}


int FirmwareInterface_Base::GetIDFor_CBRequestSLAACAddress() { return 0; }
int FirmwareInterface_Base::GetIDFor_CBSLAACServerChange() { return 0; }

Data
FirmwareInterface_Base::CMD_RequestSLAACAddress(const uint8_t prefix[], int prefix_len)
{
	throw not_implemented();
}

int
FirmwareInterface_Base::Parse_CBSLAACServerChange(const Data& data, const uint8_t **prefix, int *prefix_len, uint8_t *available)
{
	throw not_implemented();
}

Data
FirmwareInterface_Base::CMD_ConfigExternalRoute(uint8_t flags, const uint8_t route_prefix[], int prefix_len, uint8_t domain_id)
{
	throw not_implemented();
}

int FirmwareInterface_Base::GetIDFor_CBConfigExternalRoute() { return 0; }

Data
FirmwareInterface_Base::CMD_GetGlobalPrefixes()
{
	throw not_implemented();
}

int FirmwareInterface_Base::GetIDFor_CBGetGlobalPrefix() { return 0; }

int
FirmwareInterface_Base::Parse_CBGetGlobalPrefix(const Data &data, uint8_t *flags, const uint8_t **prefix, uint8_t *domain_id,
    uint32_t *preferred_lifetime, uint32_t *valid_lifetime)
{
	throw not_implemented();
}
