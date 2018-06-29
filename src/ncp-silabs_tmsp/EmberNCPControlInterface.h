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
 *		This file implements the header for code which handles communication
 *		with ember-style NCPs.
 *
 */

#ifndef __WPAN_EMBER_NCP_H__
#define __WPAN_EMBER_NCP_H__ 1

#include <queue>
#include "NCPInstance.h"
#include "NCPControlInterface.h"
#include "NCPMfgInterface_v0.h"
#include "CallbackStore.hpp"
#include "nlpt.h"
#include <set>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "Callbacks.h"
#include "EventHandler.h"
#include "EmberAPIGlue.h"
#include "EmberPacked.h"
#include "RunawayResetBackoffManager.h"

namespace nl {
namespace wpantund {

#define kWPANTUNDProperty_DebugControlProtothreadLine "Debug:ControlProtothreadLine"

class EmberNCPInstance;

class EmberNCPControlInterface : public NCPControlInterface, public NCPMfgInterface_v0, public nl::EventHandler {
public:
	class HelperTask;
	class JoinTask;
	class FormTask;
	class ScanTask;
	class LurkTask;
	class LeaveTask;
	class GetRIPEntryTask;
	class SendCommandTask;
	class MfgFinish;
	class SetNetworkKeyTask;
	class WakeupTask;
	class DeepSleepTask;
	class GetGlobalAddressesTask;
	class GetCounter;

	friend class EmberNCPInstance;

	enum DriverState {
		INITIALIZING,
		INITIALIZING_WAITING_FOR_RESET,
		NORMAL_OPERATION
	};

	enum ScanType {
		kScanTypeNetScan,
		kScanTypeEnergyScan,
	};

public:

	EmberNCPControlInterface();
	virtual ~EmberNCPControlInterface() { }

	virtual void join(
		const ValueMap& options,
	    CallbackWithStatus cb = NilReturn()
	);

	virtual void form(
		const ValueMap& options,
	    CallbackWithStatus cb = NilReturn()
	);

	virtual void leave(CallbackWithStatus cb = NilReturn());
	virtual void attach(CallbackWithStatus cb = NilReturn());
	virtual void begin_low_power(CallbackWithStatus cb = NilReturn());

	virtual void netscan_start(const ValueMap& options, CallbackWithStatus cb = NilReturn());
	virtual void netscan_stop(CallbackWithStatus cb = NilReturn());

	virtual void energyscan_start(const ValueMap& options, CallbackWithStatus cb = NilReturn());
	virtual void energyscan_stop(CallbackWithStatus cb = NilReturn());

	virtual void begin_net_wake(uint8_t data, uint32_t flags, CallbackWithStatus cb = NilReturn());
	virtual void reset(CallbackWithStatus cb = NilReturn());
	virtual void permit_join(
	    int seconds = 15 * 60,
	    uint8_t commissioning_traffic_type = 0xFF,
	    in_port_t commissioning_traffic_port = 0,
	    bool network_wide = false,
	    CallbackWithStatus      cb = NilReturn());

	virtual void refresh_state(CallbackWithStatus cb = NilReturn());

	virtual const WPAN::NetworkInstance& get_current_network_instance(void)const;

	std::set<std::string> get_supported_property_keys()const;

	virtual void property_get_value(
	    const std::string& key, CallbackWithStatusArg1 cb);
	virtual void property_set_value(
	    const std::string&                      key,
	    const boost::any&                       value,
	    CallbackWithStatus      cb);
	virtual void property_insert_value(
		const std::string& key,
		const boost::any& value,
		CallbackWithStatus cb
	);

	virtual void property_remove_value(
		const std::string& key,
		const boost::any& value,
		CallbackWithStatus cb
	);

	void disable(CallbackWithStatus cb = NilReturn());
	void enable(CallbackWithStatus cb = NilReturn());

	virtual void add_on_mesh_prefix(
		const struct in6_addr& prefix,
		uint8_t prefix_len,
		OnMeshPrefixFlags flags,
		OnMeshPrefixPriority priority,
		bool stable,
		CallbackWithStatus cb
	);

	virtual void remove_on_mesh_prefix(
		const struct in6_addr& prefix,
		uint8_t prefix_len,
		CallbackWithStatus cb
	);

	virtual void add_external_route(
		const struct in6_addr *prefix,
		int prefix_len_in_bits,
		int domain_id,
		ExternalRoutePriority priority,
		bool stable,
		CallbackWithStatus cb
	);

	virtual void remove_external_route(
		const struct in6_addr *prefix,
		int prefix_len_in_bits,
		int domain_id,
		CallbackWithStatus cb
	);

	virtual void joiner_add(
		const char *psk,
		uint32_t joiner_timeout,
		const uint8_t *addr,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void data_poll(CallbackWithStatus cb = NilReturn());
	virtual void host_did_wake(CallbackWithStatus cb = NilReturn());

	virtual void peek(uint32_t address, uint16_t count, CallbackWithStatusArg1 cb = NilReturn());
	virtual void poke(uint32_t address, Data bytes, CallbackWithStatus cb = NilReturn());

	int process(void);
	int	update_busy_indication(void);

	virtual std::string get_name();

	virtual void pcap_to_fd(int fd, CallbackWithStatus cb = NilReturn());
	virtual void pcap_terminate(CallbackWithStatus cb = NilReturn());

	/*********** BEGIN NCPMfgInterface ***********/
	virtual void mfg_start(CallbackWithStatus cb);
	virtual void mfg_finish(CallbackWithStatusArg1 cb);
	virtual void mfg_begin_test(uint8_t type, CallbackWithStatus cb);
	virtual void mfg_end_test(uint8_t type, CallbackWithStatus cb);
	virtual void mfg_tx_packet(const Data& packet, int16_t repeat, CallbackWithStatus cb);
	virtual void mfg_clockmon(bool enable, uint32_t timerId, CallbackWithStatus cb);
	virtual void mfg_gpio_set(uint8_t port_pin, uint8_t config, uint8_t value, CallbackWithStatus cb);
	virtual void mfg_gpio_get(uint8_t port_pin, CallbackWithStatusArg1 cb);
	virtual void mfg_channelcal(uint8_t channel, uint32_t duration,  CallbackWithStatus cb);
	virtual void mfg_channelcal_get(uint8_t channel, CallbackWithStatusArg1 cb);
	int vprocess_mfg(int event, va_list args);
	int8_t mMfgFirstPacketRSSI;
	uint8_t mMfgFirstPacketLQI;
	cms_t mMfgRepeatRandomTXInterval;
	uint8_t mMfgRepeatRandomTXLen;

	void handle_cb_mfg_rx(uint8_t *packet, uint8_t linkQuality, int8_t rssi);
	/*********** END NCPMfgInterface ***********/

protected:

	virtual cms_t get_ms_to_next_event(void);
	virtual int vprocess_event(int event, va_list args);

	// Helpful for use with callbacks.
	int process_event_helper(int event);

protected:

	int vprocess_init(int event, va_list args);
	int vprocess_disabled(int event, va_list args);
	int vprocess_auto_deep_sleep(int event, va_list args);
	int vprocess_duty_cycled_sleep(int event, va_list args);
	int vprocess_joined_net_wake(int event, va_list args);
	int vprocess_tickle(int event, va_list args);
	int vprocess_saved(int event, va_list args);
	int vprocess_disconnected(int event, va_list args);

	// Wake up the NCP if it is asleep.
	int wakeup(CallbackWithStatus cb = NilReturn());

	// Ends any ongoing task(s) or pending callback(s) with the given reason
	void reset_callbacks(int reason);

	// Task Management
	int start_new_helper_task(const boost::shared_ptr<HelperTask> &task);

	int start_send_command_task(
		const CallbackWithStatusArg1& cb,
		const CallbackSimple& send_command_callback,
		int callback_event = EVENT_NULL,
		int timeout = NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		EmberCommands::PacketFieldType ret_packed_type = EmberCommands::PacketFieldType::EMBER_PACKED_UNKNOWN_CHAR_T
	);

	void request_address_from_ncp(const struct in6_addr& addr, int prefix_len, int flags);
	void request_manual_address_from_ncp(const struct in6_addr& addr, int prefix_len);
	void abandon_address_from_ncp(const struct in6_addr& addr, int prefix_len);

	void config_gateway_address(uint8_t flags, const struct in6_addr& addr, int prefix_len, uint32_t preferredLifetime, uint32_t validLifetime, CallbackWithStatus cb);
	void config_external_route(uint8_t flags, const uint8_t *route_prefix, int prefix_len, uint8_t domain_id, CallbackWithStatus cb);

	void scan_start(ScanType scan_type, const ValueMap& options, CallbackWithStatus cb);
	void scan_stop(CallbackWithStatus cb);

public:
	// Handles transitioning from state-to-state.
	void handle_ncp_state_change(NCPState new_ncp_state, NCPState old_ncp_state);

	void handle_ncp_network_state_changed(void);
	void handle_ncp_reset(EmberResetCause cause);
	void handle_ncp_assert(const uint8_t *fileName, uint32_t lineNumber);
	void handle_ncp_get_versions(const uint8_t *versionName,
                            uint16_t binaryManagementVersionNumber,
                            uint16_t stackVersionNumber,
                            uint16_t stackBuildNumber,
                            EmberVersionType versionType,
                            const uint8_t *buildTimestamp);


	void process_ember_callback(int event, va_list args);
	void handle_cb_set_address(va_list args);
	void handle_cb_address_configuration_change(va_list args);
	void handle_cb_external_route_change(va_list args);
	void handle_cb_dhcp_server_change(va_list args);
	void handle_cb_request_dhcp_address(va_list args);
	void handle_cb_wakeup(va_list args);
	void handle_cb_set_network_keys(va_list args);
	void handle_cb_slaac_server_change(va_list args);

public: // Hooks from NCPInstance
	void handle_ncp_frame(uint8_t type, const uint8_t* command, size_t length);
	bool is_ok_to_sleep();

	void reinitialize_ncp(void);

	void refresh_addresses(void);
	bool is_router(void) const;

	virtual NCPInstance& get_ncp_instance(void);

public:

private:

	EmberNCPInstance* mNCPInstance;

	// Configurable Local State
	IntegerSet mActiveWakeupBlacklist;
	int mNCPTXPowerLimit;
	int mRequestedTXPower;
	bool mUseDeepSleepOnLowPower;
	bool mAlwaysResetToWake;
	float mNCPInitFlushTime;
	float mNCPInitResetWaitTime;

	// Configurable NCP State
	int mTXPower;
	int mTXPowerMode;
	int mCCAThreshold;
	int mNCPDebugging;
	uint32_t mSleepPollInterval;
	Data mNetworkKey;
	uint32_t mNetworkKeyIndex;
	in_port_t mPassthruPort; //!^ In network order
	int mTransmitHookActive;

	// Mirrored NCP state
	uint8_t mNetWakeData;
	int mManagementVersion;
	cms_t mNetWakeEnd;
	uint8_t mNetWakeState;
	bool mMfgTestMode;

	// Transient State
	DriverState mDriverState;
	bool mIsGoingIntoDeepSleep;
	bool mIsWakingUpFromSleep;
	bool mResetIsExpected;
	cms_t mLastCommandSent;
	bool mGotInitCallback;
	uint32_t mDefaultChannelMask;

	// Protothreads and related state
	PT mSleepPT;
	PT mSubPT;

	// Task management
	std::list<boost::shared_ptr<HelperTask> > mTaskQueue;

	// Kind-of deprecated, but still used by several low-level mechanisms.
	// Keeping it around for now until there is a viable alternative.
	CallbackStore<uint16_t> mCallbackStore;
};

extern class EmberNCPControlInterface *gNCPControlInterface;

// This class is the base class for tasks (like joining or forming).
// They allow you to implement complex, stateful processes without being
// too unwieldly.
class EmberNCPControlInterface::HelperTask
	: public boost::enable_shared_from_this<HelperTask>
	, public boost::signals2::trackable
	, public nl::EventHandler
{
public:
	HelperTask(EmberNCPControlInterface* _interface, CallbackWithStatusArg1 cb);
	virtual ~HelperTask();

	virtual int vprocess_event(int event, va_list args) = 0;

	virtual void finish(int status, const boost::any& value = boost::any());

	EmberNCPControlInterface* mInterface;

protected:
	int peek_at_ember_event_status(int event, va_list args);

protected:
	CallbackWithStatusArg1 mCB;
};

}; // namespace wpantund
}; // namespace nl


#endif
