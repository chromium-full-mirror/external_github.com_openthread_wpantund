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
#include "RunawayResetBackoffManager.h"

namespace nl {
namespace wpantund {

#define kWPANTUNDProperty_DebugControlProtothreadLine "Debug:ControlProtothreadLine"

class EmberNCPInstance;
namespace EmberCommands {
	class FirmwareInterface;
}

class EmberNCPControlInterface : public NCPControlInterface, public NCPMfgInterface_v0, protected nl::EventHandler {
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

	friend class EmberNCPInstance;

	enum DriverState {
		INITIALIZING,
		INITIALIZING_WAITING_FOR_RESET,
		NORMAL_OPERATION
	};

	enum ScanType
	{
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

	virtual const WPAN::NetworkInstance& get_current_network_instance()const;

	virtual void get_property(
	    const std::string& key, CallbackWithStatusArg1 cb);
	virtual void set_property(
	    const std::string&                      key,
	    const boost::any&                       value,
	    CallbackWithStatus      cb);
	void disable(CallbackWithStatus cb = NilReturn());
	void enable(CallbackWithStatus cb = NilReturn());
	virtual void add_on_mesh_prefix(
		const struct in6_addr *prefix,
		bool defaultRoute,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void remove_on_mesh_prefix(
		const struct in6_addr *prefix,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void add_external_route(
		const struct in6_addr *prefix,
		int prefix_len_in_bits,
		int domain_id,
		ExternalRoutePriority priority,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void remove_external_route(
		const struct in6_addr *prefix,
		int prefix_len_in_bits,
		int domain_id,
		CallbackWithStatus cb = NilReturn()
	);

	virtual void data_poll(CallbackWithStatus cb = NilReturn());
	virtual void host_did_wake(CallbackWithStatus cb = NilReturn());

	std::set<std::string> get_supported_property_keys()const;

	virtual void pcap_to_fd(int fd, CallbackWithStatus cb = NilReturn());

	virtual void pcap_terminate(CallbackWithStatus cb = NilReturn());

	virtual std::string get_name();

	/*********** BEGIN NCPMfgInterface ***********/
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
	void handle_cb_mfg_rx(const Data& data);
	int8_t mMfgFirstPacketRSSI;
	uint8_t mMfgFirstPacketLQI;
	cms_t mMfgRepeatRandomTXInterval;
	int mMfgRepeatRandomTXLen;
	/*********** END NCPMfgInterface ***********/

protected:

	virtual cms_t get_ms_to_next_event(void);
	virtual int vprocess_event(int event, va_list args);

	// Helpful for use with callbacks.
	int process_event_helper(int event);

protected:

	void handle_ncp_event(int event, const uint8_t* data, size_t data_len);

	int vprocess_init(int event, va_list args);
	int vprocess_disabled(int event, va_list args);
	int vprocess_auto_deep_sleep(int event, va_list args);
	int vprocess_duty_cycled_sleep(int event, va_list args);
	int vprocess_joined_net_wake(int event, va_list args);
	int vprocess_tickle(int event, va_list args);
	int vprocess_saved(int event, va_list args);
	int vprocess_disconnected(int event, va_list args);

	// Sends the given management frame to the NCP,
	// waking it up and queueing if necessary.
	int send_management_command(const Data& data, CallbackWithStatus cb = NilReturn());

	// Wake up the NCP if it is asleep.
	int wakeup(CallbackWithStatus cb = NilReturn());

	// Handles transitioning from state-to-state.
	void handle_ncp_state_change(NCPState new_ncp_state, NCPState old_ncp_state);

	// Ends any ongoing task(s) or pending callback(s) with the given reason
	void reset_callbacks(int reason);

	// Task Management
	int StartNewTask(const boost::shared_ptr<HelperTask> &task);

	void get_counter(uint8_t index, CallbackWithStatusArg1 cb);

	void handle_cb_set_address(const Data& data);
	void handle_cb_set_driver_address(const Data& data);
	void handle_cb_external_route_change(const Data& data);
	void handle_cb_dhcp_server_change(const Data& data);
	void handle_cb_request_dhcp_address(const Data& data);
	void handle_cb_permit_join(const Data& data);
	void handle_cb_listen(const Data& data);
	void handle_cb_wakeup(const Data& data);
	void handle_cb_state(const Data& data);
	void handle_cb_set_network_keys(const Data& data);
	void handle_cb_get_versions(const Data& data);
	void handle_cb_assert(const Data& data);
	void handle_cb_reset(const Data& data);
	void handle_cb_address_configuration_change(const Data& data);
	void handle_cb_slaac_server_change(const Data& data);
	void handle_cb_get_global_address(const Data& data);

	void request_address_from_ncp(const struct in6_addr& addr, int prefix_len, int flags);
	void request_manual_address_from_ncp(const struct in6_addr& addr, int prefix_len);
	void abandon_address_from_ncp(const struct in6_addr& addr, int prefix_len);

	void config_gateway_address(uint8_t flags, const struct in6_addr& addr, uint32_t preferredLifetime, uint32_t validLifetime, CallbackWithStatus cb);
	void config_external_route(uint8_t flags, const uint8_t *route_prefix, int prefix_len, uint8_t domain_id, CallbackWithStatus cb);

	void scan_start(ScanType scan_type, const ValueMap& options, CallbackWithStatus cb);
	void scan_stop(CallbackWithStatus cb);

protected: // Hooks from NCPInstance
	void handle_ncp_frame(uint8_t type, const uint8_t* command, size_t length);
	bool is_ok_to_sleep();

	void reinitialize_ncp(void);

	void refresh_addresses(void);
	bool is_router(void) const;

	virtual NCPInstance& get_ncp_instance(void);

public:

	boost::shared_ptr<EmberCommands::FirmwareInterface> mFirmwareInterface;

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
	bool mUseLegacyChannel;
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
	int GetNCPCallbackReturnStatus(int event, va_list args);

protected:
	CallbackWithStatusArg1 mCB;
};

}; // namespace wpantund
}; // namespace nl


#endif
