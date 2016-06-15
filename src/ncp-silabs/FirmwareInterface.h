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
 *      This file contains the virtual base class for the FirmwareInterface
 *      class.
 *
 */

#ifndef __wpantund__EmberCommands__
#define __wpantund__EmberCommands__

#include <stdarg.h>
#include <boost/shared_ptr.hpp>

#include "Data.h"
#include "NetworkInstance.h"
#include "NCPControlInterface.h"
#include "EmberPacked.h"
#include "wpan-error.h"
#include "NCPTypes.h"

#define EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG      0x80
#define EMBER_TYPE_SUPPORTS_LEGACY(x)               (((x)&EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG)==EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG)

namespace nl {
namespace wpantund {
namespace EmberCommands {

typedef uint16_t ncp_header_t;

Data StripNCPHeader(const Data& data);

class FirmwareInterface {
public:
	// ========================================================================
	// MARK: Public enumerations

	typedef enum {
		TONE,
		STREAM
	} mfgActivities;

	typedef enum {
		CHANNEL,
		POWER,
		POWER_MODE,
		SYN_OFFSET
	} mfgValues;

	typedef enum {
		RESET_UNKNOWN,
		RESET_FIB,
		RESET_BOOTLOADER,
		RESET_EXTERNAL,
		RESET_POWERON,
		RESET_WATCHDOG,
		RESET_SOFTWARE,
		RESET_CRASH,
		RESET_FLASH,
		RESET_FATAL,
		RESET_FAULT
	} resetCause;

	typedef enum {
		WAKEUP_STATE_LISTENING,
		WAKEUP_STATE_TRANSMITTING,
		WAKEUP_STATE_NOT_LISTENING
	} wakeupState;

	enum ConfigureGatewayFlags {
		DEFAULT_ROUTE_FLAG    = 0x02,
		CONFIGURE_FLAG        = 0x04,
		DHCP_FLAG             = 0x08,
		SLAAC_VALID_FLAG      = 0x10,
		SLAAC_PREFERRED_FLAG  = 0x20,
		HIGH_PREFERENCE       = 0x40,
		TEMPORARY_FLAG        = 0x80,
		LOW_PREFERENCE        = 0xC0,
	};

public:
	// ========================================================================
	// MARK: Constants

	static const uint32_t kAllChannelsMask;

public:
	// ========================================================================
	// MARK: Exceptions

	class not_implemented : public std::runtime_error {
	public:
		explicit not_implemented(): runtime_error("not_implemented") { };
	};

	class unsupported_firmware_version : public std::runtime_error {
	public:
		explicit unsupported_firmware_version(): runtime_error("unsupported_firmware_version") { };
	};

	class parse_error : public std::runtime_error {
	public:
		explicit parse_error(): runtime_error("parse_error") { };
	};


public:
	// ========================================================================
	// MARK: Constructor/Destructor

	static boost::shared_ptr<FirmwareInterface> GetFirmwareInterfaceForVersion(int management_version = 0, const std::string& version = "");
	virtual ~FirmwareInterface();


public:
	// ========================================================================
	// MARK: Firmware Interface Information Methods

	virtual const std::string& GetVersionString(void) = 0;
	virtual int GetManagementVersion(void) = 0;


public:
	// ========================================================================
	//MARK:  Type conversion and stringification helpers

	static const char* reset_cause_to_cstr(FirmwareInterface::resetCause reason);

	virtual NCPState NCPStateConvertFromEmber(uint8_t association_state);

	virtual NodeType NodeTypeConvertFromEmber(uint8_t node_type) = 0;
	virtual uint8_t NodeTypeConvertToEmber(NodeType node_type, bool support_legacy) = 0;

	virtual std::string CommandIdentifierToString(uint16_t command_identifier) = 0;
	virtual std::string DescribeCommandOrCallback(const uint8_t* data, size_t len) = 0;

	virtual int GetReturnStatusFromNCPFrame(const uint8_t* data, size_t len) = 0;

	virtual wpantund_status_t NCPErrorToStatus(uint8_t errorcode) = 0;

	static std::string configure_gateway_flags_to_string(uint8_t flags);


public:
	// ========================================================================
	// MARK: Base commands and callbacks

	virtual Data CMD_GetVersions() = 0;
	virtual int GetIDFor_CBGetVersions() = 0;
	virtual int Parse_CBGetVersions(const Data& data, int* management_version, std::string* version_string) = 0;

	virtual Data CMD_InitHost() = 0;
	virtual int GetIDFor_CBInitHost() = 0;

	virtual Data CMD_Reset() = 0;
	virtual int GetIDFor_CBReset() = 0;

	virtual Data CMD_GetState() = 0;
	virtual int GetIDFor_CBState() = 0;
	virtual int Parse_CBState(
		const Data& data,
		WPAN::NetworkInstance* network,
		int8_t* tx_power,
		uint8_t* raw_type,
		uint8_t* association_state,
		uint8_t hwaddr[8],
		uint8_t mesh_local_prefix[8],
		uint8_t legacy_prefix[8]
	) = 0;


public:
	// ========================================================================
	// MARK: Joining, forming, resuming, attaching, etc.

	virtual Data CMD_Join(const WPAN::NetworkInstance& network, uint8_t node_type, int tx_power) = 0;
	virtual int GetIDFor_CBJoin() = 0;

	virtual Data CMD_Form(const WPAN::NetworkId& network, uint8_t node_type,uint32_t channel_mask,const uint8_t *legacy_ula, int8_t tx_power,const uint8_t *ula) = 0;
	virtual int GetIDFor_CBForm() = 0;

	virtual Data CMD_Leave() = 0;

	virtual Data CMD_Resume() = 0;
	virtual int GetIDFor_CBResume() = 0;

	virtual bool IsSupported_CBRebootNetwork() = 0;
	virtual int GetIDFor_CBRebootNetwork() = 0;

	// ----------------------------------------------------------

	virtual Data CMD_SetSecurityParams(const uint8_t network_key[16]) = 0;
	virtual int GetIDFor_CBSetSecurityParams() = 0;

	virtual int GetIDFor_CBSetNetworkKeys() = 0;
	virtual int Parse_CBSetNetworkKeys(const Data& data, uint32_t *key_index, Data* key) = 0;

	// ----------------------------------------------------------

	virtual bool IsSupported_CMDCommissionNetwork() = 0;

	virtual Data CMD_CommissionNetwork(
		const WPAN::NetworkId& network,
		uint8_t perf_chan,
		uint32_t channel_mask,
		const uint8_t ula[8],
		const uint8_t network_key[16]
	) = 0;
	virtual int GetIDFor_CBCommissionNetwork() = 0;

	virtual Data CMD_JoinCommissioned(int8_t tx_power, uint8_t node_type) = 0;


public:
	// ========================================================================
	// MARK: Scanning

	virtual Data CMD_Scan(int period, uint32_t channel_mask = kAllChannelsMask) = 0;
	virtual int GetIDFor_CBScanReturn() = 0;

	virtual Data CMD_StopScan() = 0;

	virtual int Parse_CBActiveScan(
		const Data& data,
		WPAN::NetworkInstance* network_instance
	) = 0;
	virtual int GetIDFor_CBActiveScan() = 0;


public:
	// ========================================================================
	// MARK: Network-layer methods, properties, and callbacks

	virtual Data CMD_ConfigGateway(uint8_t flags, const uint8_t prefix[], size_t prefix_len, uint32_t preferredLifetime, uint32_t validLifetime, uint8_t domain_id=0) = 0;
	virtual int GetIDFor_CBConfigGatewayReturn() = 0;

	virtual int GetIDFor_CBSetAddress() = 0;
	virtual int Parse_CBSetAddress(const Data& data, struct in6_addr *addr) = 0;

	virtual int GetIDFor_CBExternalRouteChange() = 0;
	virtual int Parse_CBExternalRouteChange(const Data& data,
		struct in6_addr *prefix,
		uint8_t *prefixLength,
		bool *available
	) = 0;

	virtual int GetIDFor_CBAddressConfiguration() = 0;
	virtual int Parse_CBAddressConfiguration(const Data& data,
		struct in6_addr *addr,
		uint32_t *validLifetime,
		uint32_t *prefferedLifetime
	) = 0;

	virtual int GetIDFor_CBAddressConfigurationChange() = 0;
	virtual int Parse_CBAddressConfigurationChange(const Data& data, struct in6_addr* address, uint32_t* valid_lifetime, uint32_t* preferred_lifetime, uint8_t* flags) = 0;

	virtual int GetIDFor_CBSetDriverAddress() = 0;

	// ----------------------------------------------------------

	virtual Data CMD_GetGlobalAddresses(const uint8_t prefix[], int prefix_len = 8) = 0;
	virtual int GetIDFor_CBGetGlobalAddress() = 0;
	virtual int Parse_CBGetGlobalAddress(const Data& data, struct in6_addr* address, uint32_t* preferred_lifetime, uint32_t *valid_lifetime, uint8_t* flags) = 0;

	virtual Data CMD_ResignGlobalAddress(const struct in6_addr* address) = 0;
	virtual int GetIDFor_CBResignGlobalAddress() = 0;

	virtual Data CMD_GetGlobalPrefixes() = 0;
	virtual int GetIDFor_CBGetGlobalPrefix() = 0;
	virtual int Parse_CBGetGlobalPrefix(const Data &data, uint8_t *flags, const uint8_t **prefix, uint8_t *domain_id,
		uint32_t *preferred_lifetime, uint32_t *valid_lifetime) = 0;

	// ----------------------------------------------------------

	virtual Data CMD_RequestDHCPAddress(const uint8_t prefix[], int prefix_len = 8) = 0;
	virtual int GetIDFor_CBRequestDHCPAddress() = 0;
	virtual int Parse_CBRequestDHCPAddress(const Data& data, struct in6_addr *addr) = 0;

	virtual int GetIDFor_CBDHCPServerChange() = 0;
	virtual int Parse_CBDHCPServerChange(const Data& data, const uint8_t **prefix, int *prefix_len, uint8_t *available) = 0;

	// ----------------------------------------------------------

	virtual Data CMD_RequestSLAACAddress(const uint8_t prefix[], int prefix_len) = 0;
	virtual int GetIDFor_CBRequestSLAACAddress() = 0;

	virtual int GetIDFor_CBSLAACServerChange() = 0;
	virtual int Parse_CBSLAACServerChange(const Data& data, const uint8_t **prefix, int *prefix_len, uint8_t *available) = 0;

	// ----------------------------------------------------------

	virtual Data CMD_ConfigExternalRoute(uint8_t flags, const uint8_t route_prefix[], int prefix_len, uint8_t domain_id) = 0;
	virtual int GetIDFor_CBConfigExternalRoute() = 0;


public:
	// ========================================================================
	// MARK: MAC-layer methods and properties

	virtual Data CMD_DataPoll() = 0;
	virtual int GetIDFor_CBDataPoll() = 0;

	virtual Data CMD_StackDataPoll(uint32_t cms) = 0;
	virtual int GetIDFor_CBStackDataPoll() = 0;

	virtual int GetIDFor_CBChangeNodeType() = 0;
	virtual Data CMD_ChangeNodeType(uint8_t type) = 0;


public:
	// ========================================================================
	// MARK: PHY-layer methods and properties

	virtual Data CMD_GetCCAThreshold() = 0;
	virtual int GetIDFor_CBGetCCAThreshold() = 0;

	virtual Data CMD_SetCCAThreshold(int8_t threshold) = 0;
	virtual int GetIDFor_CBSetCCAThreshold() = 0;

	virtual Data CMD_SetRadioPower(int8_t power_in_dbm) = 0;
	virtual int GetIDFor_CBSetRadioPower() = 0;

	virtual Data CMD_GetTXPowerMode() = 0;
	virtual int GetIDFor_CBGetTXPowerMode() = 0;

	virtual Data CMD_SetTXPowerMode(int16_t txPowerMode) = 0;
	virtual int GetIDFor_CBSetTXPowerMode() = 0;

	virtual int GetMinTXPower() = 0;
	virtual int GetMaxTXPower() = 0;

public:
	// ========================================================================
	// MARK: Counters

	virtual Data CMD_GetCounter(uint8_t type) = 0;
	virtual int GetIDFor_CBCounters() = 0;

	virtual std::string CounterIndexToString(int x) = 0;
	virtual int StringToCounterIndex(const std::string& counter_name) = 0;
	virtual int GetCounterCount() = 0;

public:
	// ========================================================================
	// MARK: Miscelaneous

	virtual Data CMD_GetRIPEntry(uint8_t index) = 0;
	virtual int GetIDFor_CBRIPEntry() = 0;

	virtual Data CMD_Echo(const Data& payload = Data()) = 0;
	virtual int GetIDFor_CBEcho() = 0;

	virtual Data CMD_FFWakeup(void) = 0;
	virtual int GetIDFor_CBAssertInfo() = 0;


public:
	// ========================================================================
	// MARK: Manufacturing Commands

	virtual Data CMD_MfgStartTest(int8_t request_rx_callback=1) = 0;
	virtual int GetIDFor_CBMfgStartTest() = 0;

	virtual Data CMD_MfgEndTest() = 0;
	virtual int Parse_CBEndTest(const Data& data, uint32_t* rx_packet_count) = 0;

	virtual Data CMD_MfgStart(uint8_t type) = 0;
	virtual int GetIDFor_CBMfgStart() = 0;

	virtual Data CMD_MfgStop(uint8_t type) = 0;
	virtual int GetIDFor_CBMfgStop() = 0;

	virtual Data CMD_MfgTXPacket(const Data& packet, int16_t repeat) = 0;
	virtual int GetIDFor_CBMfgSendPacket() = 0;

	virtual Data CMD_MfgSetChannel(int8_t channel) = 0;
	virtual Data CMD_MfgSetSYNOffset(int8_t syn_offset) = 0;
	virtual Data CMD_MfgSetPower(int16_t power_mode, int8_t power) = 0;
	virtual int GetIDFor_CBMfgSet() = 0;

	virtual Data CMD_MfgGet(uint8_t type) = 0;
	virtual int GetIDFor_CBMfgGetChannel() = 0;
	virtual int GetIDFor_CBMfgGetPower() = 0;
	virtual int GetIDFor_CBMfgGetPowerMode() = 0;
	virtual int GetIDFor_CBMfgGetSynOffset() = 0;

	virtual Data CMD_MfgClockMon(bool enable, uint32_t timerId) = 0;
	virtual int GetIDFor_CBMfgEndTest(void) = 0;

	virtual int GetIDFor_CBMfgRX() = 0;

	// ----------------------------------------------------------

	virtual Data CMD_ChannelCal(uint8_t channel, uint32_t duration) = 0;
	virtual Data CMD_ChannelCalGet(uint8_t channel) = 0;
	virtual int GetIDFor_CBGetChannelCalToken() = 0;


public:
	// ========================================================================
	// MARK: Power Management

	virtual Data CMD_SleepNoTimer() = 0;
	virtual int GetIDFor_CBSleepNoTimer() = 0;

	virtual Data CMD_OKToNap() = 0;
	virtual int GetIDFor_CBOKToNap() = 0;


public:
	// ========================================================================
	// MARK: Nest-specific

	virtual Data CMD_SetPassthruPort(in_port_t port) = 0;

	virtual Data CMD_Listen(bool should_sleep) = 0;
	virtual int GetIDFor_CBListen() = 0;

	virtual Data CMD_StartWakeup(uint8_t data) = 0;
	virtual int GetIDFor_CBStartWakeup() = 0;

	virtual Data CMD_SetActiveWakeup(bool active_wakeup) = 0;
	virtual bool IsSupported_CMDSetActiveWakeup() = 0;
	virtual int GetIDFor_CBSetActiveWakeup() = 0;

	virtual int GetIDFor_CBWakeup() = 0;

	// ----------------------------------------------------------

	virtual Data CMD_SetTransmitHookActive(bool using_transmit_hook) = 0;
	virtual int GetIDFor_CBSetTransmitHookActive() = 0;

	virtual Data CMD_WriteNestMACAddr(const uint8_t mac_addr[8]) = 0;
	virtual int GetIDFor_CBWriteNestMACAddr() = 0;

	// ----------------------------------------------------------

	virtual Data CMD_PermitJoin(int seconds, bool network_wide) = 0;
	virtual int GetIDFor_CBPermitJoin() = 0;
	virtual int GetIDFor_CBPermitJoinReturn() = 0;

	virtual int GetIDFor_CBStartHostJoinClient() = 0;
	virtual Data CMD_HostJoinClientComplete(const uint8_t network_key[16], uint32_t network_key_index, const uint8_t mesh_local_prefix[8] = NULL) = 0;

	// ----------------------------------------------------------

	virtual Data CMD_GPIOSet(uint8_t port_pin, uint8_t config, uint8_t value) = 0;
	virtual Data CMD_GPIOGet(uint8_t port_pin) = 0;
	virtual int GetIDFor_CBGPIOCurrentConfig() = 0;
	virtual int GetIDFor_CBGPIOCurrentInput() = 0;


public:
	// ========================================================================
	// MARK: Work-around flags

	virtual bool Workaround_WaitForCBListenAfterSleepNoTimer(void) = 0;
	virtual bool Workaround_CommissionNetworkRequired(void) = 0;


public:
	// ========================================================================
	// MARK: Packet parsing helpers

	virtual int GetIDFor_CBCustomNCPToHostMessage() = 0;

	virtual int GetCMDID(const Data& x) = 0;
	virtual int GetCMDID(const uint8_t* data, size_t len) = 0;

	virtual Data WrapCustomCommand(const Data& unwrapped_command) = 0;
	virtual Data UnwrapCustomCallback(const Data& wrapped_command) = 0;
	virtual int UnwrapCustomCallback(const uint8_t*&data, size_t&len) = 0;
}; // class FirmwareInterface

}; // namespace EmberCommands
}; // namespace wpantund
}; // namespace nl

#endif /* defined(__wpantund__EmberCommands__) */
