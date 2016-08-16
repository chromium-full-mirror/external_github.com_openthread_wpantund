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

#ifndef __wpantund__EmberCommands_2_0_0__
#define __wpantund__EmberCommands_2_0_0__

#include <stdio.h>

#include <stdio.h>
#include "FirmwareInterface-1.1.3.h"
#include <stdexcept>

namespace nl {
namespace wpantund {
namespace EmberCommands {

class FirmwareInterface_2_0_0 : public FirmwareInterface_1_1_3 {
public:
	FirmwareInterface_2_0_0(int management_version, const std::string& version);
	static boost::shared_ptr<FirmwareInterface> alloc(int management_version, const std::string& version_string);

	virtual Data CMD_Form(const WPAN::NetworkId& network, uint8_t node_type,uint32_t channel_mask,const uint8_t *legacy_ula, int8_t tx_power,const uint8_t *ula);
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


	virtual Data CMD_HostJoinClientComplete(const uint8_t network_key[16], uint32_t network_key_index, const uint8_t mesh_local_prefix[8] = NULL);

	virtual Data CMD_DataPoll();
	virtual Data CMD_StackDataPoll(uint32_t cms);
	virtual Data CMD_OKToNap();
	virtual Data CMD_ConfigGateway(uint8_t flags, const uint8_t prefix[], size_t prefix_len, uint32_t preferredLifetime, uint32_t validLifetime, uint8_t domain_id=0);
	virtual Data CMD_RequestDHCPAddress(const uint8_t prefix[], int prefix_len);
	virtual Data CMD_SetActiveWakeup(bool active_wakeup);
	virtual int GetIDFor_CBSetActiveWakeup();
	virtual Data CMD_WriteNestMACAddr(const uint8_t mac_addr[8]);
	virtual Data CMD_SetTransmitHookActive(bool using_transmit_hook);
	virtual int GetIDFor_CBSetTransmitHookActive();
	virtual int GetIDFor_CBWriteNestMACAddr();
	virtual bool IsSupported_CBRebootNetwork();
	virtual int GetIDFor_CBRebootNetwork();

	virtual NodeType NodeTypeConvertFromEmber(uint8_t node_type);
	virtual uint8_t NodeTypeConvertToEmber(NodeType node_type, bool support_legacy);
	virtual std::string CommandIdentifierToString(uint16_t command_identifier);

	virtual Data CMD_JoinCommissioned(int8_t tx_power, uint8_t node_type);

	virtual bool IsSupported_CMDCommissionNetwork();
	virtual Data CMD_CommissionNetwork(
		const WPAN::NetworkId& network,
		uint8_t perf_chan,
		uint32_t channel_mask,
		const uint8_t ula[8],
		const uint8_t network_key[16]
	);
	virtual int GetIDFor_CBCommissionNetwork();

	virtual int GetReturnStatusFromNCPFrame(const uint8_t* data, size_t len);

	virtual int GetIDFor_CBExternalRouteChange();
	virtual int Parse_CBExternalRouteChange(const Data& data,
		struct in6_addr *prefix,
		uint8_t *prefixLength,
		bool *available
	);

	virtual wpantund_status_t NCPErrorToStatus(uint8_t errorcode);

	virtual int GetIDFor_CBDHCPServerChange();
	virtual int Parse_CBDHCPServerChange(const Data& data, const uint8_t **prefix, int *prefix_len, uint8_t *available);

	virtual int GetIDFor_CBAddressConfiguration();
	virtual int Parse_CBAddressConfiguration(const Data& data,
		struct in6_addr *addr,
		uint32_t *validLifetime,
		uint32_t *prefferedLifetime
	);

	virtual int GetIDFor_CBRequestDHCPAddress();
	virtual int Parse_CBRequestDHCPAddress(const Data& data, struct in6_addr *addr);

	virtual int GetIDFor_CBSetNetworkKeys();
	virtual int Parse_CBSetNetworkKeys(const Data& data, uint32_t *key_index, Data* key);
	virtual bool Workaround_WaitForCBListenAfterSleepNoTimer(void);

	virtual int GetIDFor_CBConfigGatewayReturn();
	virtual std::string CounterIndexToString(int x);
	virtual int GetCounterCount();

	virtual int GetIDFor_CBOKToNap();
	virtual int GetIDFor_CBDataPoll();
	virtual int GetIDFor_CBStackDataPoll();
};

};};};

#endif /* defined(__wpantund__EmberCommands_2_0_0__) */
