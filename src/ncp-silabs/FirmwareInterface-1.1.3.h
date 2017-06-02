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

#ifndef __wpantund__FirmwareInterface_1_1_3__
#define __wpantund__FirmwareInterface_1_1_3__

#include <stdio.h>
#include "FirmwareInterface-Base.h"
#include <stdexcept>

namespace nl {
namespace wpantund {
namespace EmberCommands {

class FirmwareInterface_1_1_3 : public FirmwareInterface_Base {
public:
	FirmwareInterface_1_1_3(int management_version, const std::string& version);
	static boost::shared_ptr<FirmwareInterface> alloc(int management_version, const std::string& version_string);

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
	virtual Data CMD_EnergyScan(int period, uint32_t channel_mask = kAllChannelsMask);
	virtual Data CMD_StopScan();
	virtual Data CMD_PermitJoin(int seconds, bool network_wide);
	virtual Data CMD_SleepNoTimer();
	virtual Data CMD_Listen(bool should_sleep);
	virtual Data CMD_StartWakeup(uint8_t data);
	virtual Data CMD_Echo(const Data& payload = Data());

	virtual Data CMD_SetActiveWakeup(bool active_wakeup);
	virtual int GetIDFor_CBSetActiveWakeup();

	virtual wpantund_status_t NCPErrorToStatus(uint8_t errorcode);

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

	virtual int GetIDFor_CBSetAddress();
	virtual int Parse_CBSetAddress(const Data& data, struct in6_addr *addr);

	// Nest-specific
	virtual Data CMD_SetTransmitHookActive(bool using_transmit_hook);
	virtual int GetIDFor_CBSetTransmitHookActive();

	virtual NodeType NodeTypeConvertFromEmber(uint8_t node_type);
	virtual uint8_t NodeTypeConvertToEmber(NodeType node_type, bool support_legacy);

	virtual std::string CommandIdentifierToString(uint16_t command_identifier);

	virtual int GetReturnStatusFromNCPFrame(const uint8_t* data, size_t len);

	virtual int Parse_CBActiveScan(
		const Data& data,
		WPAN::NetworkInstance* network_instance
	);

	virtual int Parse_CBEnergyScan(const Data& data, uint8_t *channel, int8_t *maxRssi);

	virtual int GetIDFor_CBSetNetworkKeys();
	virtual int Parse_CBSetNetworkKeys(const Data& data, uint32_t *key_index, Data* key);
	virtual bool Workaround_WaitForCBListenAfterSleepNoTimer(void);

	virtual Data CMD_FFWakeup(void);

	virtual int GetIDFor_CBActiveScan();
	virtual int GetIDFor_CBAssertInfo();
	virtual int GetIDFor_CBCounters();
	virtual int GetIDFor_CBEcho();
	virtual int GetIDFor_CBEnergyScan();
	virtual int GetIDFor_CBForm();
	virtual int GetIDFor_CBGetCCAThreshold();
	virtual int GetIDFor_CBGetChannelCalToken();
	virtual int GetIDFor_CBGetTXPowerMode();
	virtual int GetIDFor_CBGetVersions();
	virtual int GetIDFor_CBInitHost();
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
	virtual int GetIDFor_CBPermitJoin();
	virtual int GetIDFor_CBPermitJoinReturn();
	virtual int GetIDFor_CBReset();
	virtual int GetIDFor_CBResume();
	virtual int GetIDFor_CBRIPEntry();
	virtual int GetIDFor_CBScanReturn();
	virtual int GetIDFor_CBSetCCAThreshold();
	virtual int GetIDFor_CBSetRadioPower();
	virtual int GetIDFor_CBSetSecurityParams();
	virtual int GetIDFor_CBSetTXPowerMode();
	virtual int GetIDFor_CBSleepNoTimer();
	virtual int GetIDFor_CBStartHostJoinClient();
	virtual int GetIDFor_CBStartWakeup();
	virtual int GetIDFor_CBState();
	virtual int GetIDFor_CBWakeup();
	virtual int GetIDFor_CBGPIOCurrentConfig();
	virtual int GetIDFor_CBGPIOCurrentInput();

	virtual std::string CounterIndexToString(int x);
	virtual int GetCounterCount();

};

};};};

#endif /* defined(__wpantund__FirmwareInterface_1_1_3__) */
