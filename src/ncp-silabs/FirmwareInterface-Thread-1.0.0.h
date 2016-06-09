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
 */

#ifndef __wpantund__EmberCommands_Thread_1_0_0__
#define __wpantund__EmberCommands_Thread_1_0_0__

#include <stdio.h>

#include <stdio.h>
#include "FirmwareInterface-2.0.0.h"
#include <stdexcept>

namespace nl {
namespace wpantund {
namespace EmberCommands {


class FirmwareInterface_Thread_1_0_0 : public FirmwareInterface_2_0_0 {
public:
	FirmwareInterface_Thread_1_0_0(int management_version, const std::string& version);

	static boost::shared_ptr<FirmwareInterface> alloc(int management_version, const std::string& version_string);

	virtual std::string CommandIdentifierToString(uint16_t command_identifier);

	virtual Data CMD_JoinCommissioned(int8_t tx_power, uint8_t node_type);
	virtual Data CMD_Join(const WPAN::NetworkInstance& network, uint8_t node_type, int tx_power);

	virtual Data CMD_CommissionNetwork(
		const WPAN::NetworkId& network,
		uint8_t perf_chan,
		uint32_t channel_mask,
		const uint8_t ula[8],
		const uint8_t network_key[16]
	);

	virtual int Parse_CBState(
		const Data& data,
		WPAN::NetworkInstance* network,
		int8_t* tx_power,
		uint8_t* raw_type,
		uint8_t* association_state,
		uint8_t hwaddr[8],
		uint8_t mesh_local_prefix[8],
		uint8_t legacy_prefix[8]
	);

	virtual bool Workaround_CommissionNetworkRequired(void);
	virtual Data CMD_Scan(int period, uint32_t channel_mask = kAllChannelsMask);
	virtual Data CMD_PermitJoin(int seconds, bool network_wide);
	virtual Data CMD_SetTransmitHookActive(bool using_transmit_hook);

	virtual int GetIDFor_CBReset();
	virtual int GetIDFor_CBMfgEndTest(void);
	virtual int GetIDFor_CBActiveScan();
	virtual int GetIDFor_CBAssertInfo();
	virtual int GetIDFor_CBConfigGatewayReturn();
	virtual int GetIDFor_CBCounters();
	virtual int GetIDFor_CBDataPoll();
	virtual int GetIDFor_CBEcho();
	virtual int GetIDFor_CBForm();
	virtual int GetIDFor_CBGetCCAThreshold();
	virtual int GetIDFor_CBGetChannelCalToken();
	virtual int GetIDFor_CBGetTXPowerMode();
	virtual int GetIDFor_CBJoin();
	virtual int GetIDFor_CBListen();
	virtual int GetIDFor_CBMfgGetChannel();
	virtual int GetIDFor_CBMfgGetPower();
	virtual int GetIDFor_CBMfgGetPowerMode();
	virtual int GetIDFor_CBMfgGetSynOffset();
	virtual int GetIDFor_CBMfgRX();
	virtual int GetIDFor_CBMfgSendPacket();
	virtual int GetIDFor_CBMfgSet();
	virtual int GetIDFor_CBMfgStart();
	virtual int GetIDFor_CBMfgStartTest();
	virtual int GetIDFor_CBMfgStop();
	virtual int GetIDFor_CBOKToNap();
	virtual int GetIDFor_CBPermitJoin();
	virtual int GetIDFor_CBPermitJoinReturn();
	virtual int GetIDFor_CBResume();
	virtual int GetIDFor_CBRIPEntry();
	virtual int GetIDFor_CBScanReturn();
	virtual int GetIDFor_CBSetCCAThreshold();
	virtual int GetIDFor_CBSetRadioPower();
	virtual int GetIDFor_CBSetSecurityParams();
	virtual int GetIDFor_CBSetTXPowerMode();
	virtual int GetIDFor_CBSleepNoTimer();
	virtual int GetIDFor_CBStackDataPoll();
	virtual int GetIDFor_CBStartHostJoinClient();
	virtual int GetIDFor_CBStartWakeup();
	virtual int GetIDFor_CBState();
	virtual int GetIDFor_CBWakeup();
	virtual int GetIDFor_CBGPIOCurrentConfig();
	virtual int GetIDFor_CBGPIOCurrentInput();
	virtual int GetIDFor_CBCustomNCPToHostMessage();
	virtual int GetIDFor_CBAddressConfigurationChange();

	virtual int GetIDFor_CBSetDriverAddress();

	virtual wpantund_status_t NCPErrorToStatus(uint8_t errorcode);

	virtual int Parse_CBActiveScan(
		const Data& data,
		WPAN::NetworkInstance* network_instance
	);

	virtual int GetIDFor_CBChangeNodeType();
	virtual Data CMD_ChangeNodeType(uint8_t type);

	virtual Data CMD_ConfigGateway(uint8_t flags, const uint8_t prefix[], size_t prefix_len, uint32_t preferredLifetime, uint32_t validLifetime, uint8_t domain_id=0);
	virtual int Parse_CBAddressConfigurationChange(const Data& data, struct in6_addr* address, uint32_t* valid_lifetime, uint32_t* preferred_lifetime, uint8_t* flags);

	virtual Data CMD_GetGlobalAddresses(const uint8_t prefix[], int prefix_len);
	virtual int GetIDFor_CBGetGlobalAddress();

	virtual Data CMD_ResignGlobalAddress(const struct in6_addr* address);
	virtual int GetIDFor_CBResignGlobalAddress();
	virtual int Parse_CBGetGlobalAddress(const Data& data, struct in6_addr* address, uint32_t* preferred_lifetime, uint32_t* valid_lifetime, uint8_t* flags);

	virtual std::string CounterIndexToString(int x);
	virtual int GetCounterCount();
	virtual Data WrapCustomCommand(const Data& unwrapped_command);
	virtual int UnwrapCustomCallback(const uint8_t*&data, size_t&len);

	virtual Data CMD_RequestSLAACAddress(const uint8_t prefix[], int prefix_len);
	virtual int GetIDFor_CBRequestSLAACAddress();
	virtual int GetIDFor_CBSLAACServerChange();
	virtual int Parse_CBSLAACServerChange(const Data& data, const uint8_t **prefix, int *prefix_len, uint8_t *available);

	virtual Data CMD_ChannelCalGet(uint8_t channel);

	virtual Data CMD_ConfigExternalRoute(uint8_t flags, const uint8_t route_prefix[], int prefix_len, uint8_t domain_id);
	virtual int GetIDFor_CBConfigExternalRoute();

	virtual Data CMD_GetGlobalPrefixes();
	virtual int GetIDFor_CBGetGlobalPrefix();
	virtual int Parse_CBGetGlobalPrefix(const Data &data, uint8_t *flags, const uint8_t **prefix, uint8_t *domain_id,
		uint32_t *preferred_lifetime, uint32_t *valid_lifetime);
};

};};};

#endif /* defined(__wpantund__EmberCommands_Thread_1_0_0__) */
