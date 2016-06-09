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

#ifndef __wpantund__FirmwareInterface_Base__
#define __wpantund__FirmwareInterface_Base__

#include <stdio.h>
#include "FirmwareInterface.h"
#include <stdexcept>
#include "NetworkInstance.h"

namespace nl {
namespace wpantund {
namespace EmberCommands {

class FirmwareInterface_Base : public FirmwareInterface {
public:
	static boost::shared_ptr<FirmwareInterface> alloc(int management_version, const std::string& version_string);

	FirmwareInterface_Base(int management_version, const std::string& version);

	virtual const std::string& GetVersionString(void);
	virtual int GetManagementVersion(void);

	virtual ~FirmwareInterface_Base();

	virtual Data CMD_GetVersions();
	virtual int GetIDFor_CBGetVersions();
	virtual int Parse_CBGetVersions(const Data& data, int* management_version, std::string* version_string);

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

	virtual Data CMD_InitHost();
	virtual int GetIDFor_CBInitHost();

	virtual Data CMD_Reset();
	virtual int GetIDFor_CBReset();

	virtual Data CMD_GetState();
	virtual Data CMD_Join(const WPAN::NetworkInstance& network, uint8_t node_type, int tx_power);
	virtual Data CMD_Form(const WPAN::NetworkId& network, uint8_t node_type,uint32_t channel_mask,const uint8_t *legacy_ula, int8_t tx_power,const uint8_t *ula);
	virtual Data CMD_GetRIPEntry(uint8_t index);
	virtual Data CMD_GetCCAThreshold();
	virtual Data CMD_SetCCAThreshold(int8_t threshold);
	virtual Data CMD_SetRadioPower(int8_t power_in_dbm);
	virtual Data CMD_GetTXPowerMode();
	virtual Data CMD_SetTXPowerMode(int16_t txPowerMode);
	virtual Data CMD_SetPassthruPort(in_port_t port);
	virtual Data CMD_GetCounter(uint8_t type);
	virtual Data CMD_SetSecurityParams(const uint8_t network_key[16]);
	virtual Data CMD_HostJoinClientComplete(const uint8_t network_key[16], uint32_t network_key_index, const uint8_t mesh_local_prefix[8] = NULL);
	virtual Data CMD_Leave();
	virtual Data CMD_Resume();
	virtual Data CMD_Scan(int period, uint32_t channel_mask = kAllChannelsMask);
	virtual Data CMD_StopScan();
	virtual Data CMD_PermitJoin(int seconds, bool network_wide);
	virtual Data CMD_SleepNoTimer();
	virtual Data CMD_Listen(bool should_sleep);
	virtual Data CMD_StartWakeup(uint8_t data);
	virtual Data CMD_DataPoll();
	virtual Data CMD_StackDataPoll(uint32_t cms);
	virtual Data CMD_OKToNap();
	virtual Data CMD_ConfigGateway(uint8_t flags, const uint8_t prefix[], size_t prefix_len, uint32_t preferredLifetime, uint32_t validLifetime, uint8_t domain_id=0);
	virtual Data CMD_RequestDHCPAddress(const uint8_t prefix[], int prefix_len);
	virtual Data CMD_Echo(const Data& payload = Data());

	virtual int GetMinTXPower();
	virtual int GetMaxTXPower();

	virtual Data CMD_SetActiveWakeup(bool active_wakeup);
	virtual bool IsSupported_CMDSetActiveWakeup();
	virtual int GetIDFor_CBSetActiveWakeup();

	virtual bool IsSupported_CBRebootNetwork();
	virtual int GetIDFor_CBRebootNetwork();

	virtual int Parse_CBActiveScan(
		const Data& data,
		WPAN::NetworkInstance* network_instance
	);

	// MFG Commands
	virtual Data CMD_MfgStartTest(int8_t request_rx_callback=1);
	virtual Data CMD_MfgEndTest();
	virtual int Parse_CBEndTest(const Data& data, uint32_t* rx_packet_count);
	virtual Data CMD_MfgStart(uint8_t type);
	virtual Data CMD_MfgStop(uint8_t type);
	virtual Data CMD_MfgTXPacket(const Data& packet, int16_t repeat);
	virtual Data CMD_MfgSetChannel(int8_t channel);
	virtual Data CMD_MfgSetSYNOffset(int8_t syn_offset);
	virtual Data CMD_MfgSetPower(int16_t power_mode, int8_t power);
	virtual Data CMD_MfgGet(uint8_t type);
	virtual Data CMD_MfgClockMon(bool enable, uint32_t timerId);
	virtual Data CMD_GPIOSet(uint8_t port_pin, uint8_t config, uint8_t value);
	virtual Data CMD_GPIOGet(uint8_t port_pin);
	virtual Data CMD_ChannelCal(uint8_t channel, uint32_t duration);
	virtual Data CMD_ChannelCalGet(uint8_t channel);
	virtual int GetIDFor_CBMfgEndTest(void);

	// Nest-specific
	virtual Data CMD_SetTransmitHookActive(bool using_transmit_hook);
	virtual int GetIDFor_CBSetTransmitHookActive();

	virtual Data CMD_WriteNestMACAddr(const uint8_t mac_addr[8]);
	virtual int GetIDFor_CBWriteNestMACAddr();

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

	virtual int GetIDFor_CBDHCPServerChange();
	virtual int Parse_CBDHCPServerChange(const Data& data, const uint8_t **prefix, int *prefix_len, uint8_t *available);

	virtual int GetIDFor_CBAddressConfiguration();
	virtual int Parse_CBAddressConfiguration(const Data& data,
		struct in6_addr *addr,
		uint32_t *validLifetime,
		uint32_t *prefferedLifetime
	);

	virtual int GetIDFor_CBSetDriverAddress();

	virtual int GetIDFor_CBRequestDHCPAddress();
	virtual int Parse_CBRequestDHCPAddress(const Data& data, struct in6_addr *addr);

	virtual int GetIDFor_CBSetAddress();
	virtual int Parse_CBSetAddress(const Data& data, struct in6_addr *addr);

	virtual int GetIDFor_CBSetNetworkKeys();
	virtual int Parse_CBSetNetworkKeys(const Data& data, uint32_t *key_index, Data* key);

	virtual bool Workaround_WaitForCBListenAfterSleepNoTimer(void);

	virtual bool Workaround_CommissionNetworkRequired(void);

	virtual Data CMD_FFWakeup(void);

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

	virtual int Parse_CBAddressConfigurationChange(const Data& data, struct in6_addr* address, uint32_t* valid_lifetime, uint32_t* preferred_lifetime, uint8_t* flags);

	virtual wpantund_status_t NCPErrorToStatus(uint8_t errorcode);

	virtual std::string DescribeCommandOrCallback(const uint8_t* data, size_t len);

	virtual std::string CounterIndexToString(int x);
	virtual int StringToCounterIndex(const std::string& counter_name);
	virtual int GetCounterCount();

	virtual Data WrapCustomCommand(const Data& unwrapped_command);
	virtual Data UnwrapCustomCallback(const Data& wrapped_command);
	virtual int UnwrapCustomCallback(const uint8_t*&data, size_t&len);

	virtual int GetCMDID(const Data& x);
	virtual int GetCMDID(const uint8_t* data, size_t len);

	virtual Data CMD_GetGlobalAddresses(const uint8_t prefix[], int prefix_len);
	virtual int GetIDFor_CBGetGlobalAddress();
	virtual int Parse_CBGetGlobalAddress(const Data& data, struct in6_addr* address, uint32_t* preferred_lifetime, uint32_t *valid_lifetime, uint8_t* flags);

	virtual Data CMD_ResignGlobalAddress(const struct in6_addr* address);
	virtual int GetIDFor_CBResignGlobalAddress();

	static std::string CounterIndexToString_Helper(int x, const char* counter_cstr[]);

	virtual int GetIDFor_CBChangeNodeType();
	virtual Data CMD_ChangeNodeType(uint8_t type);

	virtual Data CMD_RequestSLAACAddress(const uint8_t prefix[], int prefix_len);
	virtual int GetIDFor_CBRequestSLAACAddress();
	virtual int GetIDFor_CBSLAACServerChange();
	virtual int Parse_CBSLAACServerChange(const Data& data, const uint8_t **prefix, int *prefix_len, uint8_t *available);

	virtual Data CMD_ConfigExternalRoute(uint8_t flags, const uint8_t route_prefix[], int prefix_len, uint8_t domain_id);
	virtual int GetIDFor_CBConfigExternalRoute();

	virtual Data CMD_GetGlobalPrefixes();
	virtual int GetIDFor_CBGetGlobalPrefix();
	virtual int Parse_CBGetGlobalPrefix(const Data &data, uint8_t *flags, const uint8_t **prefix, uint8_t *domain_id,
	    uint32_t *preferred_lifetime, uint32_t *valid_lifetime);

protected:
	std::string mVersionString;
	int mManagementVersion;
};

};};};

#endif /* defined(__wpantund__FirmwareInterface_Base__) */
