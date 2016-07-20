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
 *		This file implements the code which handles communication with the
 *		ember-style NCPs.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"

#include "CallbackStore.hpp"
#include "config-file.h"
#include "nlpt.h"
#include "string-utils.h"
#include "any-to.h"
#include "time-utils.h"

#include <cstring>
#include <algorithm>
#include <errno.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/time.h>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "EmberNCPControlInterface-JoinTask.h"
#include "EmberNCPControlInterface-ScanTask.h"
#include "EmberNCPControlInterface-FormTask.h"
#include "EmberNCPControlInterface-LurkTask.h"
#include "EmberNCPControlInterface-LeaveTask.h"
#include "EmberNCPControlInterface-GetRIPEntryTask.h"
#include "EmberNCPControlInterface-GetGlobalAddressesTask.h"
#include "EmberNCPControlInterface-SendCommandTask.h"
#include "EmberNCPControlInterface-MfgFinish.h"
#include "EmberNCPControlInterface-SetNetworkKeyTask.h"
#include "EmberNCPControlInterface-WakeupTask.h"
#include "EmberNCPControlInterface-DeepSleepTask.h"
#include "EmberNCPControlInterface-GetCounter.h"
#include "EmberAPIGlue.h"

__BEGIN_DECLS
#include PLATFORM_HEADER
#include "stack/config/config.h"
#include "stack/ip/host/host-address-table.h"
__END_DECLS

#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;

// ----------------------------------------------------------------------------
// MARK: -

EmberNCPControlInterface::HelperTask::HelperTask(EmberNCPControlInterface* _interface, CallbackWithStatusArg1 cb):
	mInterface(_interface), mCB(cb)
{
}

EmberNCPControlInterface::HelperTask::~HelperTask()
{
	finish(kWPANTUNDStatus_Canceled);
}

void
EmberNCPControlInterface::HelperTask::finish(int status, const boost::any& value)
{
	if (!mCB.empty()) {
		mCB(status, value);
		mCB = CallbackWithStatusArg1();
	}
}

int
EmberNCPControlInterface::HelperTask::peek_at_ember_event_status(int event, va_list args)
{
	int ret = 0;

	if (IS_EVENT_FROM_EMBER(event)) {
		va_list tmp;
		va_copy(tmp, args);
		ret = EmberGlue::status_to_wpantund_status(va_arg(tmp, int));
		va_end(tmp);
	}

	return ret;
}

// ----------------------------------------------------------------------------
// MARK: -

class EmberNCPControlInterface *nl::wpantund::gNCPControlInterface;

EmberNCPControlInterface::EmberNCPControlInterface()
{
	assert(gNCPControlInterface == NULL);
	gNCPControlInterface = this;

	mNCPDebugging = false;
	mIsGoingIntoDeepSleep = false;
	mResetIsExpected = false;
	mTXPower = 0;	// default transmit power
	mRequestedTXPower = USE_DEFAULT_TX_POWER;
	mTXPowerMode = USE_DEFAULT_TX_POWER_MODE;
	mCCAThreshold = USE_DEFAULT_CCA_THRESHOLD;
	mTransmitHookActive = USE_DEFAULT_TRANSMIT_HOOK_ACTIVE;
	mNetworkKey = nl::Data();
	mNetworkKeyIndex = 0;
	mNetWakeData = 0;
	mNetWakeEnd = 0;
	mPassthruPort = htons(11096);
	mSleepPollInterval = 5 * MSEC_PER_SEC;
	mUseDeepSleepOnLowPower = false;
	mAlwaysResetToWake = false;
	mNCPTXPowerLimit = 999;
	mIsWakingUpFromSleep = false;
	mDefaultChannelMask = EMBER_ALL_802_15_4_CHANNELS_MASK;
	mMfgTestMode = false;
	mMfgFirstPacketRSSI = 0;
	mMfgFirstPacketLQI = 0;
	mMfgRepeatRandomTXInterval = 0;
	mMfgRepeatRandomTXLen = 0;
	mNCPInstance = NULL;
	mManagementVersion = 0;
	mNetWakeState = 0;
	mDriverState = INITIALIZING;
	mLastCommandSent = 0;
	mGotInitCallback = false;

	PT_INIT(&mSleepPT);
	PT_INIT(&mSubPT);
}


int
EmberNCPControlInterface::start_new_helper_task(const boost::shared_ptr<HelperTask> &task)
{
	if (ncp_state_is_detached_from_ncp(mNCPInstance->get_ncp_state())) {
		task->finish(kWPANTUNDStatus_InvalidWhenDisabled);
	} else if (PT_SCHEDULE(task->process_event(EVENT_STARTING_TASK))) {

		if (ncp_state_is_sleeping(mNCPInstance->get_ncp_state())
			&& (dynamic_cast<const WakeupTask*>(task.get()) == NULL)
			&& !mIsWakingUpFromSleep
		) {
			start_new_helper_task(boost::shared_ptr<HelperTask>(new WakeupTask(this, NilReturn())));
		}
		mTaskQueue.push_back(task);
		return 0;
	}
	return -1;
}


int
EmberNCPControlInterface::start_send_command_task(
	const CallbackWithStatusArg1& cb,
	const CallbackSimple& send_command_callback,
	int callback_event,
	int timeout,
	EmberCommands::PacketFieldType ret_packed_type
) {
	return start_new_helper_task(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			cb,
			send_command_callback,
			callback_event,
			timeout,
			ret_packed_type
		)
	));
}


// ----------------------------------------------------------------------------
// MARK: -

void
EmberNCPControlInterface::join(
	const ValueMap& options,
    CallbackWithStatus cb
) {
    WPAN::NetworkInstance network;
    NodeType node_type = ROUTER;
	struct in6_addr mesh_local_prefix = {};

	BEGIN_TRY_FOR_CMD(cb);

	if (!mNCPInstance->mEnabled) {
		cb(kWPANTUNDStatus_InvalidWhenDisabled);
		goto bail;
	}

	if (options.count(kWPANTUNDProperty_NetworkName)) {
		network.name = any_to_string(options.at(kWPANTUNDProperty_NetworkName));
	}

	if (options.count(kWPANTUNDProperty_NetworkNodeType)) {
		node_type = string_to_node_type(any_to_string(options.at(kWPANTUNDProperty_NetworkNodeType)));

		if (node_type == UNKNOWN) {
			cb(kWPANTUNDStatus_InvalidArgument);
			goto bail;
		}
	}

	if (options.count(kWPANTUNDProperty_NCPChannel)) {
		network.channel = any_to_int(options.at(kWPANTUNDProperty_NCPChannel));
	}

	if (options.count(kWPANTUNDProperty_NetworkPANID)) {
		network.panid = any_to_int(options.at(kWPANTUNDProperty_NetworkPANID));
	} else {
		network.panid = 0xFFFF;
	}

	if (options.count(kWPANTUNDProperty_NetworkXPANID)) {
		network.set_xpanid_as_uint64(any_to_uint64(options.at(kWPANTUNDProperty_NetworkXPANID)));
	}

	if (options.count(kWPANTUNDProperty_IPv6MeshLocalPrefix)) {
		mesh_local_prefix = any_to_ipv6(options.at(kWPANTUNDProperty_IPv6MeshLocalPrefix));
	} else {
		memcpy(mesh_local_prefix.s6_addr, mNCPInstance->mNCPV6Prefix, sizeof(mNCPInstance->mNCPV6Prefix));
	}

	start_new_helper_task(boost::shared_ptr<HelperTask>(
		new JoinTask(
			this,
			boost::bind(cb,_1),
			network,
			node_type,
			(mesh_local_prefix.s6_addr[0]==0xFD)?mesh_local_prefix.s6_addr:NULL
		)
	));

	END_TRY_FOR_CMD(cb);

bail:
	return;
}

void
EmberNCPControlInterface::form(
	const ValueMap& options,
    CallbackWithStatus cb
) {
	std::string network_name;
    NodeType node_type(ROUTER);
    uint32_t channel_mask(mDefaultChannelMask);
	struct in6_addr mesh_local_prefix = {};
	struct in6_addr legacy_prefix = {};

	BEGIN_TRY_FOR_CMD(cb);

	if (!mNCPInstance->mEnabled) {
		cb(kWPANTUNDStatus_InvalidWhenDisabled);
		return;
	}

	if (!options.count(kWPANTUNDProperty_NetworkName)) {
		cb(kWPANTUNDStatus_InvalidArgument);
		return;
	}

	network_name = any_to_string(options.at(kWPANTUNDProperty_NetworkName));

	if (options.count(kWPANTUNDProperty_NetworkNodeType)) {
		node_type = string_to_node_type(any_to_string(options.at(kWPANTUNDProperty_NetworkNodeType)));
	}

	if (options.count(kWPANTUNDProperty_NCPChannelMask)) {
		channel_mask = any_to_int(options.at(kWPANTUNDProperty_NCPChannelMask));
	}

	if (options.count(kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix)) {
		legacy_prefix = any_to_ipv6(options.at(kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix));
	}

	if (options.count(kWPANTUNDProperty_IPv6MeshLocalPrefix)) {
		mesh_local_prefix = any_to_ipv6(options.at(kWPANTUNDProperty_IPv6MeshLocalPrefix));
	}

	start_new_helper_task(boost::shared_ptr<HelperTask>(
		new FormTask(
			this,
			boost::bind(cb,_1),
			network_name,
			node_type,
			channel_mask,
			(mesh_local_prefix.s6_addr[0]==0xFD)?mesh_local_prefix.s6_addr:NULL,
			(legacy_prefix.s6_addr[0]==0xFD)?legacy_prefix.s6_addr:NULL
		)
	));

	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::leave(CallbackWithStatus cb)
{
	BEGIN_TRY_FOR_CMD(cb);
	start_new_helper_task(boost::shared_ptr<HelperTask>(
		new LeaveTask(
			this,
			boost::bind(cb,_1)
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::attach(CallbackWithStatus cb)
{
	int ret = 0;

	if (!mNCPInstance->mEnabled) {
		ret = kWPANTUNDStatus_InvalidWhenDisabled;
	}

	if (ret == 0) {
		BEGIN_TRY_FOR_CMD(cb);

		start_send_command_task(
			boost::bind(cb,_1),
			emberResumeNetwork,
			EVENT_EMBER_CB_RESUME_NETWORK_RETURN
		);

		END_TRY_FOR_CMD(cb);
	} else {
		cb(ret);
	}
}

void
EmberNCPControlInterface::reset(CallbackWithStatus cb)
{
	int ret = 0;

	if (mNCPInstance->get_ncp_state() == FAULT) {
		mNCPInstance->change_ncp_state(UNINITIALIZED);
		cb(ret);
	} else {
		if (!mNCPInstance->mEnabled) {
			syslog(LOG_ERR,"NCP is disabled. Skipping reset.");
			ret = kWPANTUNDStatus_InvalidWhenDisabled;
		}

		if (ret == 0) {
			BEGIN_TRY_FOR_CMD(cb);
			start_send_command_task(
				boost::bind(cb, kWPANTUNDStatus_Ok),
				emberResetMicro,
				EVENT_EMBER_CB_RESET_MICRO_HANDLER
			);
			END_TRY_FOR_CMD(cb);
		} else {
			cb(ret);
		}
	}
}

void
EmberNCPControlInterface::begin_net_wake(uint8_t data, uint32_t flags, CallbackWithStatus cb)
{
	if (!mNCPInstance->mEnabled) {
		cb(kWPANTUNDStatus_InvalidWhenDisabled);
		return;
	}

	BEGIN_TRY_FOR_CMD(cb);
	start_send_command_task(
		boost::bind(cb,_1),
		boost::bind(emberStartWakeup, data),
		EVENT_EMBER_CB_START_WAKEUP_RETURN
	);
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::host_did_wake(CallbackWithStatus cb)
{
	bool should_immediately_begin_low_power(false);
	NCPState ncp_state = mNCPInstance->get_ncp_state();

	if (((ncp_state == DEEP_SLEEP) && mNCPInstance->can_set_ncp_power())
	 || (ncp_state == FAULT)
	) {
		// If we are in the FAULT state or we have the ability
		// to directly control the power going to the NCP, then
		// there is no need to do anything here.
		cb(0);
		return;
	}

	if (!mNCPInstance->mEnabled) {
		// Even when the NCP is disabled, the NCP can still spontaneously
		// wake up when the host wakes up, due to glitches on pins, etc.
		// We should always assume that the NCP has woken up along with
		// host, so that we can take the necessary measures to put it back
		// into deep sleep again.
		if (ncp_state == DEEP_SLEEP) {
			wakeup();
		}
		cb(0);
		return;
	}

	if (ncp_state_is_sleeping(ncp_state)
		|| (ncp_state == OFFLINE)
	) {
		should_immediately_begin_low_power = true;
	}

	if ((mNCPInstance->mNodeType == LURKER) && (ncp_state == ASSOCIATED)) {
		should_immediately_begin_low_power = true;
	}

	if (should_immediately_begin_low_power) {
		// Add the begin_low_power() command to whatever
		// callback we have.

		cb = CALLBACK_FUNC_SPLIT(
			/* Success: */ boost::bind(
				&EmberNCPControlInterface::begin_low_power,
				this,
				cb
			),
			/* Failure: */ cb
		);
	}

	if (ncp_state_is_sleeping(ncp_state)) {
		int ret = wakeup(cb);
		if (ret != 0) {
			cb(ret);
		}
	} else {
		refresh_state(cb);
	}
}

void
EmberNCPControlInterface::begin_low_power(CallbackWithStatus cb)
{
	switch (mNCPInstance->get_ncp_state()) {
	case UPGRADING:
	case UNINITIALIZED:
		cb(kWPANTUNDStatus_Busy);
		break;

	case OFFLINE:
		BEGIN_TRY_FOR_CMD(cb);
		start_new_helper_task(boost::shared_ptr<HelperTask>(new DeepSleepTask(this, boost::bind(cb,_1))));
		END_TRY_FOR_CMD(cb);
		break;

	case FAULT:
	case DEEP_SLEEP:
	case NET_WAKE_ASLEEP:
		// Already in a low-power state.
		cb(kWPANTUNDStatus_Ok);
		break;

	default:
		if (mUseDeepSleepOnLowPower) {
			start_new_helper_task(boost::shared_ptr<HelperTask>(new DeepSleepTask(this, boost::bind(cb,_1))));
		} else {
			switch(mNCPInstance->mNodeType) {
			case LURKER:
				BEGIN_TRY_FOR_CMD(cb);
				start_new_helper_task(boost::shared_ptr<HelperTask>(
					new LurkTask(
						this,
						boost::bind(cb,_1)
					)
				));
				END_TRY_FOR_CMD(cb);
				break;

			case SLEEPY_END_DEVICE:
				BEGIN_TRY_FOR_CMD(cb);

				start_send_command_task(
					boost::bind(cb,_1),
					emberOkToNap,
					EVENT_EMBER_CB_OK_TO_NAP_RETURN
				);
				END_TRY_FOR_CMD(cb);
				break;

			default:
				// Other device types don't really do low power,
				// but we don't return an error here to indicate that
				// we are already in the lowest power state for our
				// device role.
				cb(0);
				break;
			}
		}
		break;
	};

}

void
EmberNCPControlInterface::refresh_state(CallbackWithStatus cb)
{
	if (!mNCPInstance->mEnabled) {
		cb(kWPANTUNDStatus_InvalidWhenDisabled);
		return;
	}
	BEGIN_TRY_FOR_CMD(cb);
	start_send_command_task(
		boost::bind(cb,_1),
		emberState,
		EVENT_EMBER_CB_STATE_RETURN
	);
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::disable(CallbackWithStatus cb)
{
	int ret = 0;

	if (!mNCPInstance->mEnabled) {
		ret = kWPANTUNDStatus_Already;
	}

	if (ret == 0) {
		mNCPInstance->mEnabled = false;
		mMfgTestMode = false;
		reset_callbacks(kWPANTUNDStatus_Canceled);
		if (ncp_state_is_detached_from_ncp(mNCPInstance->get_ncp_state())) {
			cb(ret);
		} else {
			mCallbackStore.add(EVENT_NCP_DISABLED, cb);
		}
	} else {
		cb(ret);
	}
}

void
EmberNCPControlInterface::enable(CallbackWithStatus cb)
{
	int ret = 0;

	if (mNCPInstance->mEnabled) {
		ret = kWPANTUNDStatus_Already;
	}

	if (ret == 0) {
		mNCPInstance->mEnabled = true;
		if (ncp_state_is_detached_from_ncp(mNCPInstance->get_ncp_state())) {
			reset(cb);
		} else {
			mCallbackStore.add(EVENT_NCP_NOW_AWAKE, cb);
		}
	} else {
		cb(ret);
	}
}

void
EmberNCPControlInterface::data_poll(CallbackWithStatus cb)
{
	if (!mNCPInstance->mEnabled) {
		cb(kWPANTUNDStatus_InvalidWhenDisabled);
		return;
	}

	BEGIN_TRY_FOR_CMD(cb);
	start_send_command_task(
		boost::bind(cb,_1),
		emberPollForData,
		// We will only get a callback if the NCP isn't
		// configured to automatically poll.
		(mSleepPollInterval == 0)
			? EVENT_EMBER_CB_POLL_FOR_DATA_RETURN
			: EVENT_NULL
	);
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::config_gateway(bool defaultRoute, const uint8_t prefix[8], uint32_t preferredLifetime, uint32_t validLifetime, CallbackWithStatus cb)
{
	using namespace nl::wpantund::EmberCommands;
	uint8_t flags(0);
	struct in6_addr address;

	if (!prefix) {
		cb(kWPANTUNDStatus_InvalidArgument);
		return;
	}

	if (!mNCPInstance->mEnabled) {
		cb(kWPANTUNDStatus_InvalidWhenDisabled);
		return;
	}

	BEGIN_TRY_FOR_CMD(cb);

	if (defaultRoute) {
		flags |= EMBER_BORDER_ROUTER_DEFAULT_ROUTE_FLAG;
	}

	flags |= EMBER_BORDER_ROUTER_SLAAC_PREFERRED_FLAG | EMBER_BORDER_ROUTER_SLAAC_VALID_FLAG;

	memcpy(address.s6_addr, prefix, 8);
	memcpy(address.s6_addr + 8, mNCPInstance->mNCPHardwareAddress, 8);
	address.s6_addr[8] ^= 0x02; // flip the private-use bit on the hardware address.

	config_gateway_address(flags, address, preferredLifetime, validLifetime, cb);

	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::config_gateway_address(uint8_t flags, const struct in6_addr &address,
	uint32_t preferredLifetime, uint32_t validLifetime, CallbackWithStatus cb)
{
	syslog(LOG_INFO, "Configure Gateway: addr:\"%s\" flags:%s(0x%02x) ptime:%d vtime:%d", in6_addr_to_string(address).c_str(),EmberGlue::configure_gateway_flags_to_string(flags).c_str(), flags, preferredLifetime, validLifetime);

	// TODO: Consider making a new helper class for this command,
	//       so that we avoid needing to preserve the value in a static.
	static struct in6_addr address_static;
	address_static = address;

	start_send_command_task(
		boost::bind(cb,_1),
		boost::bind(
			emberConfigureGateway,
			flags,
			address_static.s6_addr,
			sizeof(address),
			0,
			preferredLifetime,
			validLifetime
		),
		EVENT_EMBER_CB_CONFIGURE_GATEWAY_RETURN
	);
}

void
EmberNCPControlInterface::add_external_route(const uint8_t *route, int route_prefix_len, int domain_id,
	ExternalRoutePriority priority, CallbackWithStatus cb)
{
	uint8_t flags = 0;

	switch (priority) {
		case ROUTE_HIGH_PREFERENCE:
			flags = EMBER_BORDER_ROUTER_HIGH_PREFERENCE;
			break;

		case ROUTE_LOW_PREFRENCE:
			flags = EMBER_BORDER_ROUTER_LOW_PREFERENCE;
			break;

		case ROUTE_MEDIUM_PREFERENCE:
			flags = 0;
			break;
	}
	syslog(LOG_INFO, "Adding external route with priority \"%s\"", external_route_priority_to_string(priority).c_str());
	config_external_route(flags, route, route_prefix_len, domain_id, cb);
}

void
EmberNCPControlInterface::remove_external_route(const uint8_t *route, int route_prefix_len, int domain_id, CallbackWithStatus cb)
{
	uint8_t flags = EMBER_BORDER_ROUTER_TEMPORARY_FLAG;
	syslog(LOG_INFO, "Removing external route");
	config_external_route(flags, route, route_prefix_len, domain_id, cb);
}

void
EmberNCPControlInterface::config_external_route(uint8_t flags, const uint8_t *route_prefix, int prefix_len,
	uint8_t domain_id, CallbackWithStatus cb)
{
	char addr_cstr[INET6_ADDRSTRLEN] = "::";
	static struct in6_addr addr;

	if (!route_prefix) {
		cb(-EINVAL);
		return;
	}

	if ((prefix_len < 0) || (prefix_len > 16)) {
		cb(-EINVAL);
		return;
	}

	if (!mNCPInstance->mEnabled) {
		cb(-ENETDOWN);
		return;
	}

	memset(addr.s6_addr, 0, sizeof(struct in6_addr));
	memcpy(addr.s6_addr, route_prefix, prefix_len);
	inet_ntop(AF_INET6, &addr, addr_cstr, sizeof(addr_cstr));
	syslog(LOG_INFO, "Configure External Route: Route prefix: \"%s\", prefix len: %d, domain id: %d, flags: %s (0x%02x)",
		addr_cstr, prefix_len, domain_id,
		EmberGlue::configure_gateway_flags_to_string(flags).c_str(), flags
	);

	BEGIN_TRY_FOR_CMD(cb);
	start_send_command_task(
		boost::bind(cb,_1),
		boost::bind(emberConfigureExternalRoute, flags, addr.s6_addr, prefix_len, domain_id),
		EVENT_EMBER_CB_CONFIGURE_EXTERNAL_ROUTE_RETURN
	);

	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::permit_join(
    int seconds,
    uint8_t traffic_type,
    in_port_t traffic_port,
    bool network_wide,
    CallbackWithStatus cb
    )
{
	int ret = kWPANTUNDStatus_Ok;

	if (!mNCPInstance->mEnabled) {
		ret = kWPANTUNDStatus_InvalidWhenDisabled;
		goto bail;
	}

	syslog(LOG_NOTICE, "PermitJoin: seconds=%d type=%d port=%d, netwide=%d", seconds, traffic_type, ntohs(traffic_port), network_wide);

	if (traffic_port != 0) {
		mPassthruPort = traffic_port;
	}

	ret = mNCPInstance->set_commissioniner(seconds, traffic_type, mPassthruPort);

	require_noerr(ret, bail);

	BEGIN_TRY_FOR_CMD(cb);

	// No return callback for this func, so we just call it directly.
	emberSetPassthroughPort(ntohs(mPassthruPort));

	start_send_command_task(
		boost::bind(cb,_1),
		boost::bind(emberPermitJoining, seconds),
		EVENT_EMBER_CB_PERMIT_JOINING_RETURN
	);

	END_TRY_FOR_CMD(cb);

bail:
	if (ret)
		cb(ret);
}

void
EmberNCPControlInterface::scan_start(
    ScanType scan_type,
    const ValueMap& options,
    CallbackWithStatus cb
) {
	ChannelMask channel_mask(mDefaultChannelMask);
	int period = 4;

	if (options.count(kWPANTUNDProperty_NCPChannelMask)) {
		channel_mask = any_to_int(options.at(kWPANTUNDProperty_NCPChannelMask));
	}

	BEGIN_TRY_FOR_CMD(cb);
	start_new_helper_task(boost::shared_ptr<HelperTask>(
		new ScanTask(
			this,
			boost::bind(cb,_1),
			scan_type,
			period,
			channel_mask
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::scan_stop(CallbackWithStatus cb)
{
	int ret = 0;

	if (!mNCPInstance->mEnabled) {
		ret = kWPANTUNDStatus_InvalidWhenDisabled;
	}

	BEGIN_TRY_FOR_CMD(cb);
	if (ret == 0) {
		emberStopScan();
	}
	END_TRY_FOR_CMD(cb);

	cb(ret);
}

void
EmberNCPControlInterface::netscan_start(
    const ValueMap& options,
    CallbackWithStatus cb
) {
	scan_start(kScanTypeNetScan, options, cb);
}

void
EmberNCPControlInterface::netscan_stop(CallbackWithStatus cb)
{
	scan_stop(cb);
}

void
EmberNCPControlInterface::energyscan_start(
    const ValueMap& options,
    CallbackWithStatus cb
) {
	scan_start(kScanTypeEnergyScan, options, cb);
}

void
EmberNCPControlInterface::energyscan_stop(CallbackWithStatus cb)
{
	scan_stop(cb);
}

std::string
EmberNCPControlInterface::get_name() {
	return mNCPInstance->get_name();
}


NCPInstance&
EmberNCPControlInterface::get_ncp_instance()
{
	return (*mNCPInstance);
}

const WPAN::NetworkInstance&
EmberNCPControlInterface::get_current_network_instance(void)const
{
	return mNCPInstance->get_current_network_instance();
}

// ----------------------------------------------------------------------------
// MARK: -

std::set<std::string>
EmberNCPControlInterface::get_supported_property_keys()const
{
	std::set<std::string> properties(mNCPInstance->NCPInstanceBase::get_supported_property_keys());
	int i;

	if (mMfgTestMode) {
		properties.insert(kWPANTUNDProperty_SiLabs_MfgSYNOffset);
		properties.insert(kWPANTUNDProperty_SiLabs_MfgRepeatRandomTXInterval);
		properties.insert(kWPANTUNDProperty_SiLabs_MfgRepeatRandomTXLen);
		properties.insert(kWPANTUNDProperty_SiLabs_MfgFirstPacketRSSI);
		properties.insert(kWPANTUNDProperty_SiLabs_MfgFirstPacketLQI);
	} else {
		properties.insert(kWPANTUNDProperty_NetworkKey);
		properties.insert(kWPANTUNDProperty_NetworkKeyIndex);

		properties.insert(kWPANTUNDProperty_NCPSleepyPollInterval);
		properties.insert(kWPANTUNDProperty_NCPTXPowerLimit);
		properties.insert(kWPANTUNDProperty_NCPChannelMask);

		properties.insert(kWPANTUNDProperty_DaemonAutoAssociateAfterReset);
		properties.insert(kWPANTUNDProperty_DaemonAutoDeepSleep);
		properties.insert(kWPANTUNDProperty_DaemonEnabled);
		properties.insert(kWPANTUNDProperty_DaemonReadyForHostSleep);

		properties.insert(kWPANTUNDProperty_DebugControlProtothreadLine);

		properties.insert(kWPANTUNDProperty_NestLabs_LegacyEnabled);
		properties.insert(kWPANTUNDProperty_NestLabs_NetworkAllowingJoin);
		properties.insert(kWPANTUNDProperty_NestLabs_NetworkPassthruPort);
		properties.insert(kWPANTUNDProperty_NestLabs_NetworkWakeData);
		properties.insert(kWPANTUNDProperty_NestLabs_NetworkWakeRemaining);
		properties.insert(kWPANTUNDProperty_NestLabs_HackUseDeepSleepOnLowPower);
		properties.insert(kWPANTUNDProperty_NestLabs_NetworkWakeBlacklist);
		properties.insert(kWPANTUNDProperty_NestLabs_HackAlwaysResetToWake);

		properties.insert(kWPANTUNDProperty_SiLabs_NCPAssociationState);
	}

	properties.insert(kWPANTUNDProperty_NestLabs_NCPTransmitHookActive);

	properties.insert(kWPANTUNDProperty_NCPChannel);
	properties.insert(kWPANTUNDProperty_NCPVersion);
	properties.insert(kWPANTUNDProperty_NCPCCAThreshold);
	properties.insert(kWPANTUNDProperty_NCPTXPower);

	properties.insert(kWPANTUNDProperty_NetworkIsCommissioned);

	properties.insert(kWPANTUNDProperty_SiLabs_NCPTXPowerMode);
	properties.insert(kWPANTUNDProperty_SiLabs_MfgTestMode);
	properties.insert(kWPANTUNDProperty_SiLabs_NetworkNodeType);

	properties.insert(kWPANTUNDProperty_ConfigNCPDriverName);

	for(i=0; i < EMBER_COUNTER_TYPE_COUNT; i++) {
		properties.insert(kWPANTUNDProperty_SiLabs_CounterPrefix + EmberGlue::counter_type_to_string(i));
	}

	return properties;
}

void
EmberNCPControlInterface::get_property(
    const std::string& in_key, CallbackWithStatusArg1 cb
    )
{
	std::string key = in_key;

	syslog(LOG_INFO, "get_property: key: \"%s\"", in_key.c_str());

	BEGIN_TRY_FOR_CMD(boost::bind(cb,_1,boost::any()));

	if (strcaseequal(key.c_str(), kWPANTUNDProperty_ConfigNCPDriverName)) {
		cb(0, boost::any(std::string("silabs_tmsp")));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPCCAThreshold)) {
		if (!mNCPInstance->mEnabled) {
			cb(kWPANTUNDStatus_InvalidWhenDisabled, boost::any(std::string("Not available while disabled")));
		} else {
			start_send_command_task(
				cb,
				emberGetCcaThreshold,
				EVENT_EMBER_CB_GET_CCA_THRESHOLD_RETURN,
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				EmberCommands::PacketFieldType::EMBER_PACKED_INT8_CHAR_T
			);
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_NCPTXPowerMode)) {
		if (!mNCPInstance->mEnabled) {
			cb(kWPANTUNDStatus_InvalidWhenDisabled, boost::any(std::string("Not available while disabled")));
		} else if (mMfgTestMode) {
			start_send_command_task(
				cb,
				mfglibGetPowerMode,
				EVENT_EMBER_CB_MFGLIB_GET_POWER_MODE_RETURN,
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				EmberCommands::PacketFieldType::EMBER_PACKED_UINT16_CHAR_T
			);
		} else {
			start_send_command_task(
				cb,
				emberGetTxPowerMode,
				EVENT_EMBER_CB_GET_TX_POWER_MODE_RETURN,
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				EmberCommands::PacketFieldType::EMBER_PACKED_UINT16_CHAR_T
			);
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPChannel)) {
		if (mMfgTestMode) {
			start_send_command_task(
				cb,
				mfglibGetChannel,
				EVENT_EMBER_CB_MFGLIB_GET_CHANNEL_RETURN,
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				EmberCommands::PacketFieldType::EMBER_PACKED_INT8_CHAR_T
			);
		} else {
			cb(0, boost::any((int)get_current_network_instance().channel));
		}
	} else if ((key == kWPANTUNDProperty_SiLabs_MfgSYNOffset) && mMfgTestMode) {
		start_send_command_task(
			cb,
			mfglibGetSynOffset,
			EVENT_EMBER_CB_MFGLIB_GET_SYN_OFFSET_RETURN,
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			EmberCommands::PacketFieldType::EMBER_PACKED_INT8_CHAR_T
		);
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_MfgTestMode)) {
		cb(0, boost::any(mMfgTestMode));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_MfgRepeatRandomTXInterval)) {
		cb(0, boost::any(mMfgRepeatRandomTXInterval));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_MfgRepeatRandomTXLen)) {
		cb(0, boost::any(mMfgRepeatRandomTXLen));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_MfgFirstPacketRSSI)) {
		cb(0, boost::any(mMfgFirstPacketRSSI));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_MfgFirstPacketLQI)) {
		cb(0, boost::any(mMfgFirstPacketLQI));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_HackUseDeepSleepOnLowPower)) {
		cb(0, boost::any(mUseDeepSleepOnLowPower));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NCPTransmitHookActive)) {
		if (mTransmitHookActive == USE_DEFAULT_TRANSMIT_HOOK_ACTIVE) {
			cb(0, boost::any());
		} else {
			cb(0, boost::any(bool(mTransmitHookActive)));
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPChannelMask)) {
		cb(0, boost::any(mDefaultChannelMask));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPTXPower)) {
		if (mMfgTestMode) {
			start_send_command_task(
				cb,
				mfglibGetPower,
				EVENT_EMBER_CB_MFGLIB_GET_POWER_RETURN,
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				EmberCommands::PacketFieldType::EMBER_PACKED_INT8_CHAR_T
			);
		} else {
			cb(0, boost::any(static_cast<int>(mTXPower)));
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPTXPowerLimit)) {
		cb(0, boost::any(static_cast<int>(mNCPTXPowerLimit)));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6LinkLocalAddress)) {
		struct in6_addr address;
		if (emberGetLocalIpAddress(1, reinterpret_cast<EmberIpv6Address *>(&address))) {
			cb(0, boost::any(in6_addr_to_string(address)));
		} else {
			cb(kWPANTUNDStatus_FeatureNotSupported, std::string("Property is unavailable"));
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkWakeBlacklist)) {
		cb(0, boost::any(mActiveWakeupBlacklist));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkKey)) {
		cb(0, boost::any(mNetworkKey));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPSleepyPollInterval)) {
		cb(0, boost::any(mSleepPollInterval));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkWakeData)) {
		cb(0, boost::any(mNetWakeData));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkWakeRemaining)) {
		cms_t remaining = 0;
		if (mNCPInstance->get_ncp_state() == NET_WAKE_WAKING) {
			remaining = mNetWakeEnd - time_ms();
			if (!mNetWakeEnd || remaining<0)
				remaining = 0;
		}
		cb(0, boost::any(remaining));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkKeyIndex)) {
		cb(0, boost::any(mNetworkKeyIndex));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkPassthruPort)) {
		cb(0, boost::any(ntohs(mPassthruPort)));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_HackAlwaysResetToWake)) {
		cb(0, boost::any(mAlwaysResetToWake));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DebugControlProtothreadLine)) {
		cb(0, boost::any(static_cast<int>(mControlPT.lc)));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_NetworkNodeType)) {
		EmberNetworkParameters parameters;
		emberGetNetworkParameters(&parameters);
		cb(0, boost::any(static_cast<uint8_t>(parameters.nodeType)));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_NCPAssociationState)) {
		cb(0, boost::any(static_cast<uint8_t>(emberNetworkStatus())));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_IPv6RIPEntries)) {
		if (ncp_state_is_sleeping(mNCPInstance->get_ncp_state())) {
			cb(kWPANTUNDStatus_InvalidForCurrentState, boost::any(std::string("Will not fetch RIPEntries when NCP is asleep")));
		} else {
			start_new_helper_task(boost::shared_ptr<HelperTask>(new GetRIPEntryTask(this, cb)));
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_IPv6RIPEntriesValueMap)) {
		if (ncp_state_is_sleeping(mNCPInstance->get_ncp_state())) {
			cb(kWPANTUNDStatus_InvalidForCurrentState, boost::any(std::string("Will not fetch RIPEntries when NCP is asleep")));
		} else {
			start_new_helper_task(boost::shared_ptr<HelperTask>(new GetRIPEntryTask(this, cb, GetRIPEntryTask::kOutputModeValueMapList)));
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6AllAddresses))  {
		start_new_helper_task(boost::shared_ptr<HelperTask>(new GetGlobalAddressesTask(this, cb)));

	} else if (strncaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_CounterPrefix, sizeof(kWPANTUNDProperty_SiLabs_CounterPrefix)-1)) {
		int type = EmberGlue::string_to_counter_type(std::string(key.begin()+sizeof(kWPANTUNDProperty_SiLabs_CounterPrefix)-1, key.end()));
		if (type < EMBER_COUNTER_TYPE_COUNT) {
			if (!mNCPInstance->mEnabled) {
				cb(kWPANTUNDStatus_InvalidWhenDisabled, boost::any(std::string("Not available while disabled")));
			} else {
				start_new_helper_task(boost::shared_ptr<HelperTask>(
					new GetCounter(this, cb, static_cast<uint8_t>(type))
				));
			}
		} else {
			cb(kWPANTUNDStatus_PropertyNotFound, std::string("Property not found"));
		}
	} else {
		mNCPInstance->NCPInstanceBase::get_property(in_key, cb);
	}
	END_TRY_FOR_CMD(boost::bind(cb,_1,boost::any()));
}

void
EmberNCPControlInterface::set_property(
    const std::string&                      key,
    const boost::any&                       value,
    CallbackWithStatus      cb
    )
{
	syslog(LOG_INFO, "set_property: key: \"%s\"", key.c_str());
	// If we are disabled, then the only property we
	// are allowed to set is kWPANTUNDProperty_DaemonEnabled.
	if (!mNCPInstance->mEnabled && !strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonEnabled)) {
		cb(kWPANTUNDStatus_InvalidWhenDisabled);
		return;
	}

	try {
		if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkKey)) {
			nl::Data net_key = any_to_data(value);

			start_new_helper_task(boost::shared_ptr<HelperTask>(
				new SetNetworkKeyTask(
					this,
					boost::bind(cb,_1),
					net_key,
					mNetworkKeyIndex
				)
			));
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_NetworkNodeType)) {
			start_send_command_task(
				boost::bind(cb,_1),
				boost::bind(emberChangeNodeType,any_to_int(value)),
				EVENT_EMBER_CB_CHANGE_NODE_TYPE_RETURN
			);
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkNodeType)) {
			uint8_t type = EmberGlue::node_type_convert_to_ember(string_to_node_type(any_to_string(value)), mNCPInstance->mLegacyInterfaceEnabled);
			start_send_command_task(
				boost::bind(cb,_1),
				boost::bind(emberChangeNodeType,type),
				EVENT_EMBER_CB_CHANGE_NODE_TYPE_RETURN
			);
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkKeyIndex)) {
			uint8_t index = (uint8_t)any_to_int(value);
			switch (mNCPInstance->get_ncp_state()) {
			case DEEP_SLEEP:
			case OFFLINE:
				if (!mNetworkKey.empty()) {
					cb(kWPANTUNDStatus_InvalidArgument);
					break;
				}
				if (index != 1) {
					cb(kWPANTUNDStatus_InvalidArgument);
					break;
				}
			case ASSOCIATING:
			case CREDENTIALS_NEEDED:
			case ASSOCIATED:
				mNetworkKeyIndex = index;
				cb(0);
				break;
			default:
				cb(kWPANTUNDStatus_InvalidArgument);
				break;
			}
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPTXPower)) {
			if (mMfgTestMode) {
				mRequestedTXPower = any_to_int(value);
				mTXPower = std::min(mNCPTXPowerLimit, mRequestedTXPower);
				start_send_command_task(
					boost::bind(cb,_1),
					boost::bind(mfglibSetPower,mTXPowerMode, mRequestedTXPower),
					EVENT_EMBER_CB_MFGLIB_SET_POWER_RETURN
				);

			} else {
				if (value.empty() || ((abs(any_to_int(value)) & ~0x7F) != 0)) {
					// If the given value is larger than 127 or less than -128,
					// or not set at all, then we use the default TX power of
					// the NCP.
					mRequestedTXPower = USE_DEFAULT_TX_POWER;
					cb(0);
				} else {
					mRequestedTXPower = any_to_int(value);
					mRequestedTXPower = std::min(mRequestedTXPower, EMBER_MAX_RADIO_TX_POWER);
					mRequestedTXPower = std::max(mRequestedTXPower, EMBER_MIN_RADIO_TX_POWER);

					mTXPower = std::min(mNCPTXPowerLimit, mRequestedTXPower);

					if ((mNCPInstance->get_ncp_state() != DEEP_SLEEP) && !ncp_state_is_initializing(mNCPInstance->get_ncp_state())) {
						start_send_command_task(
							boost::bind(cb,_1),
							boost::bind(emberSetRadioPower, mTXPower),
							EVENT_EMBER_CB_SET_RADIO_POWER_RETURN
						);
					} else {
						cb(0);
					}
				}
			}
			if (mRequestedTXPower == USE_DEFAULT_TX_POWER) {
				syslog(LOG_INFO, "mRequestedTXPower set to UNDEFINED");
			} else {
				syslog(LOG_INFO, "mRequestedTXPower set to %d mdB", mTXPower);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPSleepyPollInterval)) {
			mSleepPollInterval = (uint32_t)any_to_int(value);
			if (ncp_state_is_interface_up(mNCPInstance->get_ncp_state())) {
				start_send_command_task(
					boost::bind(cb,_1),
					boost::bind(emberStackPollForData,mSleepPollInterval),
					EVENT_EMBER_CB_STACK_POLL_FOR_DATA_RETURN
				);
			} else {
				cb(0);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NCPTransmitHookActive)) {
			if (value.empty()) {
				mTransmitHookActive = USE_DEFAULT_TRANSMIT_HOOK_ACTIVE;
				cb(0);
			} else {
				mTransmitHookActive = any_to_int(value);
				if ((mNCPInstance->get_ncp_state() != DEEP_SLEEP) && (mDriverState == NORMAL_OPERATION)) {
					start_send_command_task(
						boost::bind(cb,_1),
						boost::bind(nestEmberSetTransmitHookActive,any_to_bool(value)),
						EVENT_EMBER_CB_SET_TRANSMIT_HOOK_ACTIVE_RETURN
					);
				} else {
					cb(0);
				}
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPHardwareAddress)) {
			Data value_as_data;
			if (!value.empty()) {
				value_as_data = any_to_data(value);
			}
			if ((value_as_data.size() != 0) && (value_as_data.size() != 8)) {
				throw std::invalid_argument("Hardware address must be 8 bytes long");
			}
			start_new_helper_task(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					boost::bind(cb,_1),
					EmberCommands::PackArguments( // TODO: Move this over to ember API
						EMBER_PACKED_UINT16 "b",
						EMBER_WRITE_NEST_MAC_ADDR_COMMAND_IDENTIFIER,
						8,
						value_as_data.data()
					),
					CB_WRITE_NEST_MAC_ADDR_COMMAND_IDENTIFIER
				)
			));

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPCCAThreshold)) {
			if (value.empty()) {
				mCCAThreshold = USE_DEFAULT_CCA_THRESHOLD;
				cb(0);
			} else {
				mCCAThreshold = any_to_int(value);
				if ((mNCPInstance->get_ncp_state() != DEEP_SLEEP) && !ncp_state_is_initializing(mNCPInstance->get_ncp_state())) {
					start_send_command_task(
						boost::bind(cb,_1),
						boost::bind(emberSetCcaThreshold, any_to_int(value)),
						EVENT_EMBER_CB_SET_CCA_THRESHOLD_RETURN
					);
				} else {
					cb(0);
				}
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPChannelMask)) {
			uint32_t mask = (uint32_t)any_to_int(value);
			if (mask & ~EMBER_ALL_802_15_4_CHANNELS_MASK) {
				cb(kWPANTUNDStatus_InvalidRange);
			} else {
				if (mask == 0) {
					mask = ~(uint32_t)EMBER_ALL_802_15_4_CHANNELS_MASK;
				}
				mDefaultChannelMask = mask;
				cb(0);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_HackAlwaysResetToWake)) {
			mAlwaysResetToWake = any_to_bool(value);
			cb(0);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_MfgRepeatRandomTXInterval)) {
			mMfgRepeatRandomTXInterval = any_to_int(value);
			cb(0);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_MfgRepeatRandomTXLen)) {
			mMfgRepeatRandomTXLen = any_to_int(value);
			cb(0);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6MeshLocalAddress)) {
			if (mNCPInstance->get_ncp_state() <= OFFLINE) {
				nl::Data prefix = any_to_data(value);
				if (prefix.size() != sizeof(mNCPInstance->mNCPV6Prefix)) {
					// Prefix is the wrong length.
					cb(kWPANTUNDStatus_InvalidArgument);
				}
				memcpy(mNCPInstance->mNCPV6Prefix, prefix.data(), sizeof(mNCPInstance->mNCPV6Prefix));
				cb(0);
			} else {
				cb(kWPANTUNDStatus_InvalidArgument);
			}

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkWakeBlacklist)) {
			mActiveWakeupBlacklist = any_to_int_set(value);
			cb(0);

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPTXPowerLimit)) {
			mNCPTXPowerLimit = any_to_int(value);
			if ((mNCPInstance->get_ncp_state() != DEEP_SLEEP)
				&& !ncp_state_is_initializing(mNCPInstance->get_ncp_state())
			) {
				if (mRequestedTXPower != USE_DEFAULT_TX_POWER) {
					mTXPower = mRequestedTXPower;
				}
				mTXPower = std::min(mNCPTXPowerLimit,mTXPower);
				start_send_command_task(
					boost::bind(cb,_1),
					boost::bind(emberSetRadioPower,mTXPower),
					EVENT_EMBER_CB_SET_RADIO_POWER_RETURN
				);
			} else {
				cb(0);
			}
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_HackUseDeepSleepOnLowPower)) {
			mUseDeepSleepOnLowPower = any_to_bool(value);
			cb(0);
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_LegacyEnabled)) {
			EmberNetworkParameters parameters;
			emberGetNetworkParameters(&parameters);

			mNCPInstance->mLegacyInterfaceEnabled = any_to_bool(value);

			if ( mNCPInstance->mLegacyInterfaceEnabled
			  && ncp_state_is_associated(mNCPInstance->get_ncp_state())
			  && !EMBER_TYPE_SUPPORTS_LEGACY(parameters.nodeType)
			) {
				start_send_command_task(
					boost::bind(cb, _1),
					boost::bind(emberChangeNodeType, parameters.nodeType | EMBER_NODE_TYPE_LURKER_COMPATIBLE_FLAG)
				);
			} else {
				cb(0);
			}
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkPassthruPort)) {
			mPassthruPort = htons(any_to_int(value));
			if ((mNCPInstance->get_ncp_state() != DEEP_SLEEP)
			 && !ncp_state_is_initializing(mNCPInstance->get_ncp_state())
			 && !ncp_state_is_detached_from_ncp(mNCPInstance->get_ncp_state())
			 && mNCPInstance->get_current_network_instance().joinable
			) {
				start_send_command_task(
					boost::bind(cb,_1),
					boost::bind(emberSetPassthroughPort,ntohs(mPassthruPort))
				);
			} else {
				cb(0);
			}
		} else if ((key == kWPANTUNDProperty_SiLabs_MfgSYNOffset) && mMfgTestMode) {
			start_send_command_task(
				boost::bind(cb,_1),
				boost::bind(mfglibSetSynOffset, (int8_t)any_to_int(value))
			);
		} else if ((key == kWPANTUNDProperty_NCPChannel) && mMfgTestMode) {
			start_send_command_task(
				boost::bind(cb,_1),
				boost::bind(mfglibSetChannel,(int8_t)any_to_int(value)),
				EVENT_EMBER_CB_MFGLIB_SET_CHANNEL_RETURN
			);
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_MfgTestMode)) {
			bool bool_value = any_to_bool(value);
			if (mMfgTestMode != bool_value) {
				mMfgTestMode = bool_value;

				if (mMfgTestMode) {
					mfg_start(cb);
				} else {
					mfg_finish(boost::bind(cb,_1));
				}

			} else {
				cb(0);
			}
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_NCPTXPowerMode)) {
			if (mMfgTestMode) {
				mTXPowerMode = any_to_int(value);
				start_send_command_task(
					boost::bind(cb,_1),
					boost::bind(mfglibSetPower,mTXPowerMode, mRequestedTXPower),
					EVENT_EMBER_CB_MFGLIB_SET_POWER_RETURN
				);
			} else {
				if (value.empty()) {
					mTXPowerMode = USE_DEFAULT_TX_POWER_MODE;
					cb(0);
				} else {
					mTXPowerMode = any_to_int(value);
					if ((mNCPInstance->get_ncp_state() != DEEP_SLEEP) && !ncp_state_is_initializing(mNCPInstance->get_ncp_state())) {
						start_send_command_task(
							boost::bind(cb,_1),
							boost::bind(emberSetTxPowerMode, any_to_int(value)),
							EMBER_SET_TX_POWER_MODE_COMMAND_IDENTIFIER
						);
					} else {
						cb(0);
					}
				}
			}
		} else {
			mNCPInstance->NCPInstanceBase::set_property(key, value, cb);
		}
	} catch (const boost::bad_any_cast &x) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR,"set_property: Bad type for property \"%s\" (%s)", key.c_str(), x.what());
		cb(kWPANTUNDStatus_InvalidArgument);
	} catch (const std::invalid_argument &x) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR,"set_property: Invalid argument for property \"%s\" (%s)", key.c_str(), x.what());
		cb(kWPANTUNDStatus_InvalidArgument);
	}
}

// ----------------------------------------------------------------------------
// MARK: -

void
EmberNCPControlInterface::handle_ncp_network_state_changed()
{
	NCPState new_ncp_state;
	EmberNetworkParameters networkParams;
	emberGetNetworkParameters(&networkParams);

	WPAN::NetworkInstance new_network_instance(EmberGlue::network_instance_from_ember_network_parameters(&networkParams));

	if (mNCPInstance->mNCPVersionString.empty()) {
		// Until we have the version from the NCP,
		// we are not allowed to handle any other callbacks.
		return;
	}

	if (!mNCPInstance->mEnabled && mNCPInstance->get_ncp_state() != UNINITIALIZED) {
		// If we are disabled, we don't handle this event.
		return;
	}

	// determine whether to carry over the old joinable state over or not
	new_network_instance.joinable = (mNCPInstance->get_current_network_instance().joinable
		&& (static_cast<const WPAN::NetworkId &>(new_network_instance) == mNCPInstance->get_current_network_instance()));

	syslog(
		LOG_INFO,
		"NCP Status: State=%d, CH=%d, TXP=%d, raw_node_type=0x%02X",
		emberNetworkStatus(),
		networkParams.channel,
		networkParams.radioTxPower,
		networkParams.nodeType
	);

	if (0 != memcmprev(mNCPInstance->mNCPHardwareAddress, emberEui64(), sizeof(mNCPInstance->mNCPHardwareAddress))) {
		uint8_t hardware_address[sizeof(mNCPInstance->mNCPHardwareAddress)];
		memcpyrev(&hardware_address, emberEui64(), sizeof(hardware_address));
		mNCPInstance->set_hardware_address(hardware_address);
	}

	if (0 != memcmp(mNCPInstance->mNCPV6Prefix, networkParams.ulaPrefix.bytes, sizeof(mNCPInstance->mNCPV6Prefix))) {
		// mNCPInstance->mNCPV6Prefix is a union with mNCPMeshLocalAddress.
		emberGetLocalIpAddress(0, reinterpret_cast<EmberIpv6Address *>(mNCPInstance->mNCPMeshLocalAddress.s6_addr));

		memcpy(mNCPInstance->mNCPV6Prefix, networkParams.ulaPrefix.bytes, sizeof(mNCPInstance->mNCPV6Prefix));

		syslog(
			LOG_INFO,
			"NCP Status: Mesh-Local Prefix: %02X%02X:%02X%02X:%02X%02X:%02X%02X::/64",
			mNCPInstance->mNCPV6Prefix[0],mNCPInstance->mNCPV6Prefix[1],mNCPInstance->mNCPV6Prefix[2],mNCPInstance->mNCPV6Prefix[3],
			mNCPInstance->mNCPV6Prefix[4],mNCPInstance->mNCPV6Prefix[5],mNCPInstance->mNCPV6Prefix[6],mNCPInstance->mNCPV6Prefix[7]
		);
		mOnPropertyChanged(kWPANTUNDProperty_IPv6MeshLocalPrefix, Data(mNCPInstance->mNCPV6Prefix, sizeof(mNCPInstance->mNCPV6Prefix)));
		mOnPropertyChanged(kWPANTUNDProperty_IPv6MeshLocalAddress, in6_addr_to_string(mNCPInstance->mNCPMeshLocalAddress));
	}

	mNCPInstance->mNodeTypeSupportsLegacy = EMBER_TYPE_SUPPORTS_LEGACY(networkParams.nodeType);

	if (mNCPInstance->mNodeTypeSupportsLegacy) {
		if (0 != memcmp(mNCPInstance->mNCPV6LegacyPrefix, networkParams.legacyUla.bytes, sizeof(mNCPInstance->mNCPV6LegacyPrefix))) {
			memcpy(mNCPInstance->mNCPV6LegacyPrefix, networkParams.legacyUla.bytes, sizeof(mNCPInstance->mNCPV6LegacyPrefix));
			mOnPropertyChanged(kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix, Data(mNCPInstance->mNCPV6LegacyPrefix, sizeof(mNCPInstance->mNCPV6LegacyPrefix)));
			syslog(
				LOG_INFO,
				"NCP Status: Legacy Prefix:     %02X%02X:%02X%02X:%02X%02X:%02X%02X::/64",
				mNCPInstance->mNCPV6LegacyPrefix[0],mNCPInstance->mNCPV6LegacyPrefix[1],mNCPInstance->mNCPV6LegacyPrefix[2],mNCPInstance->mNCPV6LegacyPrefix[3],
				mNCPInstance->mNCPV6LegacyPrefix[4],mNCPInstance->mNCPV6LegacyPrefix[5],mNCPInstance->mNCPV6LegacyPrefix[6],mNCPInstance->mNCPV6LegacyPrefix[7]
			);
		}
		mNCPInstance->enable_legacy_interface();
	}

	if (mNCPInstance->mNodeType != EmberGlue::node_type_convert_from_ember(networkParams.nodeType)) {
		mNCPInstance->mNodeType = EmberGlue::node_type_convert_from_ember(networkParams.nodeType);
		mOnPropertyChanged(
			kWPANTUNDProperty_NetworkNodeType,
			node_type_to_string(mNCPInstance->mNodeType)
		);
	}

	if (mTXPower != networkParams.radioTxPower) {
		mTXPower = networkParams.radioTxPower;

		// Actively limit transmit power
		if ((mTXPower > mNCPTXPowerLimit)
			&& mNCPInstance->mEnabled
			&& !mIsGoingIntoDeepSleep
			&& (mDriverState == NORMAL_OPERATION)
		) {
			syslog(LOG_NOTICE, "Limiting TXPower to %ddBm (was %ddBm)", mNCPTXPowerLimit, mTXPower);

			mTXPower = mNCPTXPowerLimit;

			start_send_command_task(
				NilReturn(),
				boost::bind(emberSetRadioPower, mTXPower),
				EVENT_EMBER_CB_SET_RADIO_POWER_RETURN
			);
		}

		mOnPropertyChanged(
			kWPANTUNDProperty_NCPTXPower,
			mTXPower
		);
	}

	new_ncp_state = EmberGlue::ncp_state_convert_from_ember(emberNetworkStatus());
	switch (new_ncp_state) {
	default:
		break;

	case ASSOCIATING:
		if (mNCPInstance->get_ncp_state() == CREDENTIALS_NEEDED) {
			new_ncp_state = CREDENTIALS_NEEDED;
		}
		break;

	case ASSOCIATED:
		if (mNCPInstance->get_ncp_state() == NET_WAKE_WAKING) {
			new_ncp_state = NET_WAKE_WAKING;
		}
		break;

	case ISOLATED:
		if (mNCPInstance->get_ncp_state() == NET_WAKE_WAKING) {
			new_ncp_state = NET_WAKE_WAKING;
		}
		break;

	case FAULT:
		assert_printf("Unknown EmberNetworkStatus %d, maybe this firmware is incompatible?", emberNetworkStatus());
		break;
	}

	if (new_ncp_state != CREDENTIALS_NEEDED) {
		mNCPInstance->mCurrentNetworkInstance = new_network_instance;
	}

	mNCPInstance->change_ncp_state(new_ncp_state);

	refresh_addresses();
}


void
EmberNCPControlInterface::handle_ncp_get_versions(const uint8_t *versionName,
                            uint16_t management_version,
                            uint16_t stackVersionNumber,
                            uint16_t stackBuildNumber,
                            EmberVersionType versionType,
                            const uint8_t *buildTimestamp)
{
	std::string version_string;
	char version_cstr[256];
	const char* version_type_cstr = NULL;

	uint8_t version_major = ((stackVersionNumber >> 12) & 0xF);
	uint8_t version_minor = ((stackVersionNumber >> 8) & 0xF);
	uint8_t version_patch = ((stackVersionNumber >> 4) & 0xF);
	uint8_t version_special = ((stackVersionNumber >> 0) & 0xF);

	switch (versionType) {
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

	snprintf(
		version_cstr,
		sizeof(version_cstr),
		"%s/%d.%d.%db%d s%d m%d.%d%s; %s",
		versionName,
		version_major,
		version_minor,
		version_patch,
		stackBuildNumber,
		version_special,
		management_version >> 8,
		management_version & 0xFF,
		version_type_cstr,
		buildTimestamp
	);

	version_string = version_cstr;

	require(
		mNCPInstance->set_ncp_version_string(version_string) == kWPANTUNDStatus_Ok,
		bail
	);

	// The above command can change our state to the upgrading state.
	// If this has occured, then we should skip the management version
	// check.
	if (ncp_state_is_detached_from_ncp(mNCPInstance->get_ncp_state())) {
		goto bail;
	}

	if (management_version != mManagementVersion) {
		mManagementVersion = management_version;

		if ( ((EMBER_MANAGEMENT_VERSION >> 8) == 0x0D)
		  && (version_major == EMBER_MAJOR_VERSION)
		  && (version_minor == EMBER_MINOR_VERSION)
		  && (version_patch < EMBER_PATCH_VERSION)
		) {
			// Work-around for the fact that SiLabs released
			// several incompatible builds with the same major management
			// version number. Setting this to zero here will
			// trigger the version mismatch fault below.
			management_version = 0;
		}

		if ((management_version >> 8) != (EMBER_MANAGEMENT_VERSION >> 8)) {
			syslog(LOG_ERR, "NCP FIRMWARE VERSION IS UNSUPPORTED");
			mNCPInstance->change_ncp_state(FAULT);

		} else if (mManagementVersion != EMBER_MANAGEMENT_VERSION) {
			syslog(
				LOG_WARNING,
				"Protocol management minor version differs (%d != %d)",
				mManagementVersion & 0xFF,
				EMBER_MANAGEMENT_VERSION & 0xFF
			);
		}
	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_ncp_assert(const uint8_t *fileName, uint32_t lineNumber)
{
	syslog(LOG_ERR,"[NCP->] ASSERT %s:%d", fileName, lineNumber);
}

void
EmberNCPControlInterface::handle_ncp_reset(EmberResetCause cause)
{
	// A reset occured.
	const char* reason_cstr = EmberGlue::reset_cause_to_cstr(cause);
	int severity = LOG_NOTICE;

	switch(cause) {
	case EMBER_RESET_FIB:
	case EMBER_RESET_BOOTLOADER:
	case EMBER_RESET_EXTERNAL:
	case EMBER_RESET_POWERON:
	case EMBER_RESET_SOFTWARE:
		severity = LOG_NOTICE;
		break;

	case EMBER_RESET_FLASH:
		severity = LOG_WARNING;
		break;

	default:
		severity = LOG_ERR;
		break;
	}
	syslog(severity, "[NCP->] RESET (Reason %d, \"%s\")", cause, reason_cstr);

	mGotInitCallback = false;

	// Cached network key is always cleared upon reset.
	mNetworkKey = Data();
	mNetworkKeyIndex = 0;
	memset(mNCPInstance->mNCPV6LegacyPrefix, 0, sizeof(mNCPInstance->mNCPV6LegacyPrefix));
	memset(mNCPInstance->mNCPMeshLocalAddress.s6_addr, 0, sizeof(mNCPInstance->mNCPMeshLocalAddress));

	if (!mResetIsExpected || (severity != LOG_NOTICE)) {
		// Unexpected chip reset.
		mNCPInstance->mRunawayResetBackoffManager.count_unexpected_reset();
		reset_callbacks(kWPANTUNDStatus_NCP_Crashed);
	}

	mResetIsExpected = false;

	// Restart initialization
	if (mDriverState == NORMAL_OPERATION) {
		reinitialize_ncp();
	}
}

void
EmberNCPControlInterface::handle_ncp_frame(uint8_t type, const uint8_t* data, size_t len)
{
	if ((len >= 2) && (type == MGNT_MSG_CMD)) {
		EmberGlue::unwrap_custom_callback(data, len);
		process_event(EmberGlue::get_cmd_id(data, len), data, len);
	}
}



// MARK: -

void
EmberNCPControlInterface::process_ember_callback(int event, va_list args)
{
	// If this method is called, we are no longer asleep.
	// Go ahead and change states.
	if ((mNCPInstance->get_ncp_state() == DEEP_SLEEP)
		&& !mIsGoingIntoDeepSleep
		&& !mResetIsExpected
	) {
		syslog(LOG_NOTICE, "NCP has awoken from deep sleep");
		mNCPInstance->change_ncp_state(UNINITIALIZED);

	} else if (mNCPInstance->get_ncp_state() == NET_WAKE_ASLEEP) {
		syslog(LOG_NOTICE, "NCP has awoken");
		mNCPInstance->change_ncp_state(ASSOCIATED);
	}

	if (mNCPInstance->mNCPVersionString.empty()) {
		// Until we have the version from the NCP,
		// we are not allowed to handle any other callbacks.
		goto bail;
	}

	if (mResetIsExpected
		&& (event != EVENT_EMBER_CB_WAKEUP_HANDLER)
	) {
		// We ignore any other command identifiers while an
		// internal reset is pending.
		goto bail;
	}

	if (!mNCPInstance->mEnabled) {
		// We ignore pretty much everything if we are disabled and initialized.
		// We make an exception for the state callback during initialization
		// because it is required for initialization to complete successfully.
		goto bail;
	}

	switch (event) {
	case EVENT_EMBER_CB_LISTEN_FOR_WAKEUP_RETURN:
		if (va_arg_small(args, EmberStatus) == 0) {
			mNetWakeState = EMBER_WAKEUP_STATE_LISTENING;
		}
		break;

	case EVENT_EMBER_CB_ADDRESS_CONFIGURATION_CHANGE:
	case EVENT_EMBER_CB_CONFIGURE_GLOBAL_HOST_ADDRESS:
		handle_cb_address_configuration_change(args);
		break;

	case EVENT_EMBER_CB_PERMIT_JOINING_HANDLER:
		mNCPInstance->mCurrentNetworkInstance.joinable = va_arg_small(args, bool);
		syslog(LOG_NOTICE, "Legacy Joining is %spermitted", mNCPInstance->mCurrentNetworkInstance.joinable?"":"NOT ");
		break;

	case EVENT_EMBER_CB_EXTERNAL_ROUTE_CHANGE:
		handle_cb_external_route_change(args);
		break;

	case EVENT_EMBER_CB_DHCP_SERVER_CHANGE:
		handle_cb_dhcp_server_change(args);
		break;

	case EVENT_EMBER_CB_REQUEST_DHCP_ADDRESS_RETURN:
		handle_cb_request_dhcp_address(args);
		break;

	case EVENT_EMBER_CB_WAKEUP_HANDLER:
		handle_cb_wakeup(args);
		break;

	case EVENT_EMBER_CB_SET_NETWORK_KEYS_HANDLER:
		handle_cb_set_network_keys(args);
		break;

	case EVENT_EMBER_CB_SLAAC_SERVER_CHANGE:
		handle_cb_slaac_server_change(args);
		break;

	case EVENT_EMBER_CB_CONFIGURE_DEFAULT_HOST_ADDRESS:
		handle_cb_set_address(args);
		break;

	default:
		break;
	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_set_address(va_list args)
{
	const EmberIpv6Address *mlAddress(va_arg(args, const EmberIpv6Address *));

	memcpy(mNCPInstance->mNCPMeshLocalAddress.s6_addr, mlAddress->bytes, sizeof(struct in6_addr));

	syslog(LOG_NOTICE, "[NCP->] Mesh-local Address is \"%s\"\n", in6_addr_to_string(mNCPInstance->mNCPMeshLocalAddress).c_str());

	refresh_addresses();
	return;
}

void
EmberNCPControlInterface::handle_cb_external_route_change(va_list args)
{
	const uint8_t *prefix = va_arg(args, const uint8_t*);
	uint8_t prefixLength = va_arg_small(args, uint8_t);
	bool available = va_arg_small(args, bool);

	struct in6_addr border_router_prefix = {};
	std::string prefix_string;

	require_action(prefixLength <= 16, bail, mNCPInstance->change_ncp_state(FAULT));

	memcpy(border_router_prefix.s6_addr, prefix, prefixLength);

	prefix_string = in6_addr_to_string(border_router_prefix);

	prefixLength *= 8;	// Convert bytes to bits

	if (available == true) {
		syslog(LOG_NOTICE, "[NCP->] Available Border Router %s/%u\n",
			prefix_string.c_str(),
			prefixLength);
		mNCPInstance->mPrimaryInterface->add_route(&border_router_prefix, prefixLength);
	} else {
		syslog(LOG_NOTICE, "[NCP->] Not Available Border Router %s/%u\n",
			prefix_string.c_str(),
			prefixLength);
		mNCPInstance->mPrimaryInterface->remove_route(&border_router_prefix, prefixLength);
	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_address_configuration_change(va_list args)
{
	const struct in6_addr &address(*va_arg(args, const struct in6_addr *));
	uint32_t valid_lifetime(va_arg(args, uint32_t));
	uint32_t preferred_lifetime(va_arg(args, uint32_t));
	uint8_t flags(va_arg_small(args, uint8_t));

	std::string address_string;

	address_string = in6_addr_to_string(address);


	if (buffer_is_nonzero(address.s6_addr, 8)) {
		syslog(
			LOG_NOTICE,
			"[NCP->] Address Config Change: \"%s\" valid:%d preferred:%d flags:%s(0x%02X)\n",
			address_string.c_str(),
			valid_lifetime,
			preferred_lifetime,
			address_flags_to_string(flags).c_str(),
			flags
		);

		mNCPInstance->update_global_address(address, valid_lifetime, preferred_lifetime, flags);
	} else {
		syslog(
			LOG_ERR,
			"[NCP->] *INVALID* Address Config Change: \"%s\" valid:%d preferred:%d flags:%s(0x%02X)\n",
			address_string.c_str(),
			valid_lifetime,
			preferred_lifetime,
			address_flags_to_string(flags).c_str(),
			flags
		);

	}
}

void
EmberNCPControlInterface::handle_cb_dhcp_server_change(va_list args)
{
	const uint8_t *prefix = va_arg(args, const uint8_t*);
	uint8_t prefix_len = va_arg_small(args, uint8_t);
	bool available = va_arg_small(args, bool);
	struct in6_addr address = {};

	require_action(prefix_len <= 16, bail, mNCPInstance->change_ncp_state(FAULT));

	memcpy(address.s6_addr, prefix, prefix_len);

	{
		std::string prefix_string(in6_addr_to_string(address));

		syslog(LOG_NOTICE, "[NCP->] DHCP Server for prefix \"%s/%u\" is %savailable", prefix_string.c_str(), prefix_len * 8, available?"":"NOT ");
	}

	if (available && (prefix_len == 8)) {
		request_address_from_ncp(address, prefix_len * 8, GA_DHCP);
	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_request_dhcp_address(va_list args)
{
	EmberStatus status = va_arg_small(args, EmberStatus);
	const uint8_t *prefix = va_arg(args, const uint8_t*);
	uint8_t prefix_len = va_arg_small(args, uint8_t);

	if (status != EMBER_SUCCESS) {
		syslog(LOG_NOTICE, "[NCP->] DHCP Address request error: 0x%02X (%d)\n", status, status);
	} else {
		struct in6_addr address;
		char address_string[INET6_ADDRSTRLEN] = "::";

		require_action(prefix_len <= 16, bail, mNCPInstance->change_ncp_state(FAULT));

		memcpy(address.s6_addr, prefix, prefix_len);

		inet_ntop(
			AF_INET6,
			&address,
			address_string,
			sizeof(address_string)
		);
		syslog(LOG_NOTICE, "[NCP->] DHCP Address Request from server \"%s\" returned success.\n", address_string);
	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_slaac_server_change(va_list args)
{
	const uint8_t *prefix = va_arg(args, const uint8_t*);
	uint8_t prefix_len = va_arg_small(args, uint8_t);
	bool available = va_arg_small(args, bool);
	struct in6_addr address = {};
	std::string address_string;

	require_action(prefix_len <= 16, bail, mNCPInstance->change_ncp_state(FAULT));
	memcpy(address.s6_addr, prefix, prefix_len);

	address_string = in6_addr_to_string(address);

	if (buffer_is_nonzero(prefix, prefix_len)) {
		syslog(LOG_NOTICE, "[NCP->] SLAAC Server for prefix \"%s/%u\" is %savailable", address_string.c_str(), prefix_len * 8, available?"":"NOT ");

		if (available
			&& (prefix_len == 8)
			&& !mNCPInstance->lookup_address_for_prefix(NULL, address, 64)
		) {
			memcpy(address.s6_addr + 8, mNCPInstance->mNCPHardwareAddress, 8);
			address.s6_addr[8] ^= 0x02; // flip the private-use bit on the hardware address.
			request_address_from_ncp(address, 128, 0);
		}
	} else {
		syslog(LOG_ERR, "[NCP->] *INVALID* SLAAC Server prefix \"%s/%u\" is %savailable", address_string.c_str(), prefix_len * 8, available?"":"NOT ");
	}


bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_wakeup(va_list args)
{
	EmberWakeupReason reason = va_arg_small(args, EmberWakeupReason);
	EmberWakeupState state = va_arg_small(args, EmberWakeupState);
	uint16_t ms_remaining = va_arg_small(args, uint16_t);
	uint8_t dataByte = va_arg_small(args, uint8_t);
	uint16_t ota_sequence = va_arg_small(args, uint16_t);

	// Unused Arguments.
	(void)reason;
	(void)ota_sequence;

	mNetWakeState = state;

	if (state == EMBER_WAKEUP_STATE_TRANSMITTING) {
		cms_t remaining = ms_remaining;
		mNetWakeData = dataByte;
		mNetWakeEnd = time_ms() + remaining;

		syslog(LOG_NOTICE,"WAKEUP: In-progress, %dms remaining, data: 0x%02X (%d)",remaining, mNetWakeData, mNetWakeData);
		if ((0 != remaining)
		 && (mNCPInstance->get_ncp_state() != NET_WAKE_WAKING)
		) {
			mNCPInstance->change_ncp_state(NET_WAKE_WAKING);

			if (mActiveWakeupBlacklist.count(mNetWakeData) == 0) {
				syslog(LOG_INFO,"WAKEUP: Wakeup reason %d not masked, I WILL PROPAGATE!", mNetWakeData);
				start_send_command_task(
					NilReturn(),
					boost::bind(emberActiveWakeup,true),
					EVENT_EMBER_CB_ACTIVE_WAKEUP_RETURN
				);
			} else {
				syslog(LOG_INFO,"WAKEUP: Wakeup reason %d IS masked, I will NOT propagate.", mNetWakeData);
			}
		}
		mOnNetWake(mNetWakeData, remaining);
	} else {
		syslog(LOG_NOTICE,"WAKEUP: Idle and %slistening, data: 0x%02X (%d)",
			state == EMBER_WAKEUP_STATE_NOT_LISTENING ? "NOT " : "",
			mNetWakeData, mNetWakeData
		);
		if (mNCPInstance->get_ncp_state() == NET_WAKE_WAKING) {
			start_send_command_task(
				NilReturn(),
				boost::bind(emberActiveWakeup,false),
				EVENT_EMBER_CB_ACTIVE_WAKEUP_RETURN
			);

			// Indicate that the wake is over.
			mOnNetWake(mNetWakeData, 0);

			mNCPInstance->change_ncp_state(ASSOCIATED);
		}
	}

}

void
EmberNCPControlInterface::handle_cb_set_network_keys(va_list args)
{
	uint32_t network_key_index(va_arg(args, uint32_t));
	Data network_key(va_arg(args, const uint8_t*), EMBER_ENCRYPTION_KEY_SIZE);

	if ((network_key != mNetworkKey) || (network_key_index != mNetworkKeyIndex)) {
		mNetworkKey = network_key;
		mNetworkKeyIndex = network_key_index;

#if DEBUG
		char key_string[mNetworkKey.size() * 2 + 5];

		encode_data_into_string(mNetworkKey.data(), mNetworkKey.size(), key_string,
								sizeof(key_string), (int)mNetworkKey.size() * 2);
#else
		static const char key_string[] = "<OMITTED>";
#endif

		syslog(LOG_DEBUG | LOG_AUTHPRIV,
			   "Network Key set to %s, index %d",
			   key_string,
			   mNetworkKeyIndex);
	}
}


// ----------------------------------------------------------------------------
// MARK: -

void
EmberNCPControlInterface::handle_ncp_state_change(NCPState new_ncp_state, NCPState old_ncp_state)
{
	// Interface Down -> Interface Up
	if (!ncp_state_is_interface_up(old_ncp_state)
	 && ncp_state_is_interface_up(new_ncp_state)
	) {
		start_send_command_task(
			NilReturn(),
			boost::bind(emberSetPassthroughPort,ntohs(mPassthruPort))
		);

		if ((SLEEPY_END_DEVICE == mNCPInstance->mNodeType)
			&& (mSleepPollInterval > 0)
		) {
			start_send_command_task(
				NilReturn(),
				boost::bind(emberStackPollForData,mSleepPollInterval),
				EVENT_EMBER_CB_STACK_POLL_FOR_DATA_RETURN
			);
		}

	// InterfaceUp -> COMMISSIONED
	// (Special case of InterfaceUp -> InterfaceDown)
	} else if (ncp_state_is_interface_up(old_ncp_state)
				&& (new_ncp_state == COMMISSIONED)
				&& mNCPInstance->mAutoResume
	) {
		// We don't bother handling this transition if autoresume is on.
		return;
	}

	// (Offline or Asleep) -> (Associated and Online)
	if ((!ncp_state_has_joined(old_ncp_state) || old_ncp_state==NET_WAKE_ASLEEP)
		&& ncp_state_has_joined(new_ncp_state)
		&& ncp_state_is_interface_up(new_ncp_state)
		&& mNCPInstance->mEnabled // Avoid starting new tasks when we are disabled.
	) {
		if (mNCPInstance->mLegacyInterfaceEnabled) {
			syslog(LOG_INFO, "Listening for wakeups and alarms");
			start_send_command_task(
				NilReturn(),
				boost::bind(emberListenForWakeup, EMBER_NO_SLEEP),
				EVENT_EMBER_CB_LISTEN_FOR_WAKEUP_RETURN
			);

			start_send_command_task(
				NilReturn(),
				boost::bind(emberActiveWakeup,false),
				EVENT_EMBER_CB_ACTIVE_WAKEUP_RETURN
			);
		}

		if ((mNCPInstance->mNodeType == SLEEPY_END_DEVICE) && (mSleepPollInterval > 0)) {
			start_send_command_task(
				NilReturn(),
				boost::bind(emberStackPollForData,mSleepPollInterval),
				EVENT_EMBER_CB_STACK_POLL_FOR_DATA_RETURN
			);
		}
	}

	if (ncp_state_is_sleeping(old_ncp_state)) {
		mIsWakingUpFromSleep = false;
	}
}

void
EmberNCPControlInterface::refresh_addresses(void)
{
	struct in6_addr legacy_addr;

	if (buffer_is_nonzero(mNCPInstance->mNCPV6LegacyPrefix, sizeof(mNCPInstance->mNCPV6LegacyPrefix))) {
		memcpy(&legacy_addr, mNCPInstance->mNCPV6LegacyPrefix, sizeof(mNCPInstance->mNCPV6LegacyPrefix));
		memcpy(&legacy_addr.s6_addr[8], mNCPInstance->mNCPHardwareAddress, sizeof(mNCPInstance->mNCPHardwareAddress));
		legacy_addr.s6_addr[8] ^= 0x02; // Flip the private-use bit on the hardware address.
	} else {
		memset(legacy_addr.s6_addr, 0, sizeof(legacy_addr));
	}

	if (!buffer_is_nonzero(mNCPInstance->mNCPV6Prefix, 8) && !buffer_is_nonzero(mNCPInstance->mNCPV6LegacyPrefix, 8)) {
		mNCPInstance->mPrimaryInterface->set_realm_local_address(NULL);
		return;
	}

	if (memcmp(mNCPInstance->mNCPV6LegacyPrefix, mNCPInstance->mNCPV6Prefix, sizeof(mNCPInstance->mNCPV6Prefix)) == 0) {
		if (static_cast<bool>(mNCPInstance->mLegacyInterface)) {
			mNCPInstance->mLegacyInterface->remove_address(&legacy_addr);
		}
		memset(mNCPInstance->mNCPV6LegacyPrefix, 0, sizeof(mNCPInstance->mNCPV6LegacyPrefix));
	} else {
		if (mNCPInstance->is_legacy_interface_enabled()) {
			mNCPInstance->mLegacyInterface->set_realm_local_address(&legacy_addr);
		}
	}

	if (buffer_is_nonzero(&mNCPInstance->mNCPMeshLocalAddress.s6_addr[8], 8)) {
		mNCPInstance->mPrimaryInterface->set_realm_local_address(&mNCPInstance->mNCPMeshLocalAddress);
	} else {
		mNCPInstance->mPrimaryInterface->set_realm_local_address(NULL);
	}
}

bool
EmberNCPControlInterface::is_router(void) const
{
	bool is_router;

	switch (mNCPInstance->mNodeType)
	{
	case ROUTER:
		is_router = true;
		break;

	default:
		is_router = false;
		break;
	}

	return is_router;
}

void
EmberNCPControlInterface::reset_callbacks(int reason)
{
	mCallbackStore.handle_all(reason);
	while(!mTaskQueue.empty()) {
		mTaskQueue.front()->finish(reason);
		mTaskQueue.pop_front();
	}
}

void
EmberNCPControlInterface::request_address_from_ncp(const struct in6_addr& addr, int prefix_len, int flags)
{
	int len_in_bytes((prefix_len+7)/8);
	std::string address_string(in6_addr_to_string(addr));

	if ((flags & GA_DHCP) == GA_DHCP) {
		syslog(LOG_NOTICE, "Requesting \"%s\" from NCP (DHCP)", address_string.c_str());

		// Can't queue this in a helper because of the address would be out of scope
		emberRequestDhcpAddress(addr.s6_addr, len_in_bytes);
	} else {
		syslog(LOG_NOTICE, "Requesting \"%s\" from NCP (SLAAC)", address_string.c_str());

		// Can't queue this in a helper because of the address would be out of scope
		emberRequestSlaacAddress(addr.s6_addr, len_in_bytes);
	}
}

void
EmberNCPControlInterface::request_manual_address_from_ncp(const struct in6_addr& addr, int prefix_len)
{
	using namespace nl::wpantund::EmberCommands;
	if (is_router()) {
		const uint32_t kForeverLifetime = 0xFFFFFFFF;
		// Router roles always register a prefix by configuring a gateway
		config_gateway_address(EMBER_BORDER_ROUTER_SLAAC_PREFERRED_FLAG | EMBER_BORDER_ROUTER_SLAAC_VALID_FLAG,
			addr, kForeverLifetime, kForeverLifetime, NilReturn());
	} else {
		// non-Router roles do the usual thing
		request_address_from_ncp(addr, 128, 0);
	}
}


void
EmberNCPControlInterface::abandon_address_from_ncp(const struct in6_addr& addr, int prefix_len)
{
	emberResignGlobalAddress(reinterpret_cast<const EmberIpv6Address *>(&addr));
}

bool
EmberNCPControlInterface::is_ok_to_sleep() {
	bool ok_to_sleep = false;

	switch(mNCPInstance->get_ncp_state()) {
	case DEEP_SLEEP:
	case OFFLINE:
	case NET_WAKE_ASLEEP:
	case ISOLATED:
	case ASSOCIATED:
	case FAULT:
		ok_to_sleep = true;
		break;

	default:
		break;
	}

	if (mNCPInstance->get_ncp_state() == FAULT) {
		return true;
	}

	// Exceptions to the above state-based rules.
	if (mNCPInstance->outbound_command_is_in_flight()
		|| mNCPInstance->inbound_command_is_in_flight()
		|| mIsGoingIntoDeepSleep
		|| mResetIsExpected
		|| mIsWakingUpFromSleep
		|| !mTaskQueue.empty()
		|| (mNCPInstance->get_upgrade_status() == EINPROGRESS)
	) {
		ok_to_sleep = false;
	}

	return ok_to_sleep;
}

void
EmberNCPControlInterface::reinitialize_ncp(void)
{
	PT_INIT(&mControlPT);
	mGotInitCallback = false;
	mNCPInstance->change_ncp_state(UNINITIALIZED);
}

int
EmberNCPControlInterface::wakeup(CallbackWithStatus cb)
{
	// If this function returns NON-ZERO, then the callback WAS NOT CALLED.
	int ret = 0;

	if (ncp_state_is_detached_from_ncp(mNCPInstance->get_ncp_state())) {
		return -1;
	}

	mNCPInstance->set_ncp_power(true);

	if (ncp_state_is_sleeping(mNCPInstance->get_ncp_state())) {
		start_new_helper_task(boost::shared_ptr<HelperTask>(
			new WakeupTask(
				this,
				boost::bind(cb,_1)
			)
		));
	}

	return ret;
}

// MARK: -

int
EmberNCPControlInterface::vprocess_disabled(int event, va_list args)
{
	EH_BEGIN_SUB(&mSubPT);


	while(!mNCPInstance->mEnabled) {
		// If the association state is uninitialized, fail early.
		if ((mNCPInstance->get_ncp_state() == UNINITIALIZED) || mIsWakingUpFromSleep) {
			syslog(LOG_NOTICE, "Cannot attempt to sleep until NCP is initialized.");
			EH_EXIT();
		}

		// Wait for any tasks or commands to complete.
		EH_REQUIRE_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			mNCPInstance->mEnabled || (!mNCPInstance->outbound_command_is_in_flight() && mTaskQueue.empty()),
			timeout_error
		);

		if (mNCPInstance->mEnabled) {
			break;
		}

		mMfgTestMode = false;

		if ((mNCPInstance->get_ncp_state() != DEEP_SLEEP) && (mNCPInstance->get_ncp_state() != FAULT)) {
			start_new_helper_task(boost::shared_ptr<HelperTask>(new DeepSleepTask(this, NilReturn())));

			EH_WAIT_UNTIL_WITH_TIMEOUT(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				(mNCPInstance->get_ncp_state() == DEEP_SLEEP) || mTaskQueue.empty()
			);
		}

		// If we didn't enter deep sleep then we need to bail early.
		if ((mNCPInstance->get_ncp_state() != DEEP_SLEEP) && (mNCPInstance->get_ncp_state() != FAULT)) {
			if (!ncp_state_is_initializing(mNCPInstance->get_ncp_state())) {
				emberResetMicro();
			}
			EH_EXIT();
		}

		mCallbackStore.handle(EVENT_NCP_DISABLED, 0, NULL, 0);

		EH_WAIT_UNTIL(!IS_EVENT_FROM_EMBER(event));

		EH_REQUIRE_WITHIN(
			NCP_DEEP_SLEEP_TICKLE_TIMEOUT,
			(mNCPInstance->get_ncp_state() != DEEP_SLEEP)
			|| mNCPInstance->mEnabled
			|| IS_EVENT_FROM_EMBER(event),
			do_deep_sleep_tickle
		);

		continue;

		// Exceptions

do_deep_sleep_tickle:
		// ...Go ahead and reset the NCP and state machine.
		// This will make sure the NCP is in a known state
		// and allow it to recover if the NCP is
		// unresponsive.
		syslog(LOG_WARNING, "DEEP-SLEEP-TICKLE: Resetting NCP . . .");

		// Send a RESET.
		emberResetMicro();

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			timeout_error
		);

		mResetIsExpected = true;

		// Wait for the response.
		EH_REQUIRE_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			event == EVENT_EMBER_CB_RESET_MICRO_HANDLER,
			timeout_error
		);
		mResetIsExpected = false;
		continue;

timeout_error:
		EH_EXIT();
	}

	wakeup();

	EH_END();
}

int
EmberNCPControlInterface::vprocess_auto_deep_sleep(int event, va_list args)
{
	EH_BEGIN_SUB(&mSubPT);

	// If the association state is uninitialized, fail early.
	if ((mNCPInstance->get_ncp_state() == UNINITIALIZED) || mIsWakingUpFromSleep) {
		syslog(LOG_WARNING, "Can't auto deep sleep while NCP is uninitialized.");
		EH_EXIT();
	}

	// Wait for any tasks or commands to complete.
	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		!mNCPInstance->outbound_command_is_in_flight() && mTaskQueue.empty(),
		timeout_error
	);

	// Enter deep sleep.
	start_new_helper_task(boost::shared_ptr<HelperTask>(new DeepSleepTask(this, NilReturn())));

	EH_WAIT_UNTIL_WITH_TIMEOUT(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		!mIsGoingIntoDeepSleep
	);

	// If we didn't enter deep sleep then we need to bail early.
	if (mNCPInstance->get_ncp_state() != DEEP_SLEEP) {
		if (!ncp_state_is_initializing(mNCPInstance->get_ncp_state())) {
			// We only need to reset if the NCP isn't UNINITIALIZED.
			emberResetMicro();
			CONTROL_REQUIRE_FLUSH_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				timeout_error
			);
		}
		EH_EXIT();
	}

	EH_YIELD();

	// Wait for us to wake up.
	EH_REQUIRE_WITHIN(
		NCP_DEEP_SLEEP_TICKLE_TIMEOUT,
		!(mNCPInstance->get_ncp_state() == DEEP_SLEEP)
		|| !mNCPInstance->mEnabled
		|| mResetIsExpected
		|| (IS_EVENT_FROM_EMBER(event) || IS_EVENT_FROM_NCP(event)),
		do_deep_sleep_tickle
	);

	// If we were just disabled, then go back to
	// the top of the main loop
	if (!mNCPInstance->mEnabled) {
		EH_EXIT();
	}

	EH_YIELD();

	// If we are at this point, we woke up because
	// mResetIsExpected was set. First we wait for
	// an event from the NCP.
	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		(IS_EVENT_FROM_EMBER(event) || IS_EVENT_FROM_NCP(event)),
		timeout_error
	);

	// Wait for mResetIsExpected to clear. This will
	// just fall thru if mResetIsExpected is already clear.
	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		!mResetIsExpected,
		timeout_error
	);

	syslog(LOG_NOTICE, "Finished waking NCP");
	mCallbackStore.handle(EVENT_NCP_NOW_AWAKE, 0, NULL, 0);
	EH_EXIT();

do_deep_sleep_tickle:
	// ...Go ahead and reset the NCP and state machine.
	// This will make sure the NCP is in a known state
	// and allow it to recover if the NCP is
	// unresponsive.
	syslog(LOG_WARNING, "DEEP-SLEEP-TICKLE: Resetting NCP . . .");
	emberResetMicro();
	EH_EXIT();

timeout_error:
	syslog(LOG_ERR, "NCP is misbehaving or unresponsive");
	EH_EXIT();

	EH_END();
}

int
EmberNCPControlInterface::vprocess_joined_net_wake(int event, va_list args)
{
	// Conditions under which this protothread should be exited gracefully.
	const bool should_exit = !mNCPInstance->mEnabled || mMfgTestMode;

	if (event == EVENT_EMBER_CB_LISTEN_FOR_WAKEUP_RETURN) {
		if (va_arg_small(args, EmberStatus) == 0) {
			mNetWakeState = EMBER_WAKEUP_STATE_LISTENING;
		}
	}

	EH_BEGIN_SUB(&mSubPT);

	syslog(LOG_NOTICE, "NET WAKE: 0x%02X, %dms remaining", mNetWakeData, mNetWakeEnd-time_ms());

	// Require that the network wake end within 30 seconds.
	EH_REQUIRE_WITHIN(
		30,
		(mNCPInstance->get_ncp_state() != NET_WAKE_WAKING) || should_exit,
		on_error
	);

	// Go back to listening.
	if (!should_exit && ncp_state_has_joined(mNCPInstance->get_ncp_state())) {
		// SiLabs recommends pausing briefly before
		// sending the LISTEN command.
		EH_SLEEP_FOR(0.1);

		emberListenForWakeup(EMBER_NO_SLEEP);

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			on_error
		);
	}

	EH_EXIT();

on_error:

	syslog(LOG_ERR, "NCP is misbehaving or unresponsive");

	reinitialize_ncp();

	EH_END();
}

int
EmberNCPControlInterface::vprocess_saved(int event, va_list args)
{
	// Conditions under which this protothread should be exited gracefully.
	const bool should_exit = !mNCPInstance->mEnabled
		|| (mNCPInstance->get_ncp_state() != COMMISSIONED)
		|| mMfgTestMode;

	EH_BEGIN_SUB(&mSubPT);

	EH_WAIT_UNTIL(should_exit || mNCPInstance->mAutoResume);

	if (should_exit) {
		EH_EXIT();
	}

	syslog(LOG_NOTICE, "AutoResuming NCP");

	emberResumeNetwork();

	CONTROL_REQUIRE_FLUSH_WITHIN(
		NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
		on_error
	);

	EH_WAIT_UNTIL_WITH_TIMEOUT(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		should_exit
		|| EVENT_EMBER_CB_RESUME_NETWORK_RETURN
		|| ncp_state_has_joined(mNCPInstance->get_ncp_state())
	);

	if (eh_did_timeout) {
		emberState();

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			on_error
		);
	}

	EH_EXIT();

on_error:

	syslog(LOG_ERR, "NCP is misbehaving or unresponsive");

	reinitialize_ncp();

	EH_END();
}

int
EmberNCPControlInterface::vprocess_disconnected(int event, va_list args)
{
	// Conditions under which this protothread should be exited gracefully.
	const bool should_exit = ncp_state_is_interface_up(mNCPInstance->get_ncp_state())
		|| mNCPInstance->outbound_command_is_in_flight()
		|| !mNCPInstance->mEnabled
		|| mMfgTestMode;

	float sleep_timeout = mNCPInstance->mAutoDeepSleepTimeout;

	if (!mNetworkKey.empty() || (mNetworkKeyIndex != 0)) {
		sleep_timeout += 60;
	}

	EH_BEGIN_SUB(&mSubPT);

	// Wait for auto deep sleep to be turned on, or if there is an exit condition.
	EH_WAIT_UNTIL(should_exit || mNCPInstance->mAutoDeepSleep);

	// Wait for auto deep sleep to be turned off or for us to be not asleep, or if there is an exit condition
	EH_WAIT_UNTIL(
	   should_exit
	   || !mNCPInstance->mAutoDeepSleep
	   || !ncp_state_is_sleeping(mNCPInstance->get_ncp_state())
	);

	// Wait within a timeout for us to enter sleep state, or for auto deep sleep to be turned off, or
	// if we have a command to send to NCP or receive a callback/event from NCP, or if there is an exit condition
	EH_WAIT_UNTIL_WITH_TIMEOUT(
		sleep_timeout,
		should_exit
		|| !mNCPInstance->mAutoDeepSleep
		|| !mTaskQueue.empty()
		|| (IS_EVENT_FROM_EMBER(event) || IS_EVENT_FROM_NCP(event))
		|| ncp_state_is_sleeping(mNCPInstance->get_ncp_state())
	);

	if (eh_did_timeout) {
		start_new_helper_task(boost::shared_ptr<HelperTask>(new DeepSleepTask(this, NilReturn())));
	}

	EH_END();
}

int
EmberNCPControlInterface::vprocess_tickle(int event, va_list args)
{
	// Conditions under which this protothread should be exited gracefully.
	const bool should_exit = !mNCPInstance->mEnabled
		|| !ncp_state_is_joining_or_joined(mNCPInstance->get_ncp_state())
		|| (mNCPInstance->get_ncp_state() == NET_WAKE_WAKING)
		|| (mNCPInstance->get_ncp_state() == NET_WAKE_ASLEEP)
		|| mMfgTestMode;


	EH_BEGIN_SUB(&mSubPT);

	EH_WAIT_UNTIL_WITH_TIMEOUT(
		NCP_TICKLE_TIMEOUT,
		should_exit || !(IS_EVENT_FROM_NCP(event) || IS_EVENT_FROM_EMBER(event))
	);

	// This is not a typo. Despite the above "WAIT_UNTIL" looking
	// very similar, it is not the same! DO NOT REMOVE!
	EH_WAIT_UNTIL_WITH_TIMEOUT(
		NCP_TICKLE_TIMEOUT,
		should_exit || (IS_EVENT_FROM_NCP(event) || IS_EVENT_FROM_EMBER(event))
	);

	if (eh_did_timeout) {
		syslog(LOG_INFO, "Tickle...");
		emberState();
		EH_REQUIRE_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			should_exit || (IS_EVENT_FROM_NCP(event) || IS_EVENT_FROM_EMBER(event)),
			on_error
		);
		mNCPInstance->mFailureCount = 0;
	}

	EH_EXIT();

on_error:

	syslog(LOG_ERR, "NCP is misbehaving or unresponsive");
	reinitialize_ncp();

	EH_END();
}

int
EmberNCPControlInterface::vprocess_duty_cycled_sleep(int event, va_list args)
{
	// Conditions under which this protothread should be exited gracefully.
	const bool should_exit = !mNCPInstance->mEnabled
		|| mMfgTestMode;

	EH_BEGIN_SUB(&mSubPT);

	EH_WAIT_UNTIL(
		should_exit
		|| (mNCPInstance->get_ncp_state() != NET_WAKE_ASLEEP)
		|| mIsWakingUpFromSleep
	);

	if (mIsWakingUpFromSleep) {
		// In this case we are about to transition out of this state.
		EH_REQUIRE_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			should_exit
			|| (mNCPInstance->get_ncp_state() != NET_WAKE_ASLEEP)
			|| !mIsWakingUpFromSleep,
			on_error
		);
	}

	EH_EXIT();

on_error:

	syslog(LOG_ERR, "NCP is misbehaving or unresponsive");
	reinitialize_ncp();

	EH_END();
}

int
EmberNCPControlInterface::vprocess_init(int event, va_list args)
{
	if (event == EVENT_EMBER_CB_INIT_RETURN) {
		mGotInitCallback = true;
	}

	if (event == EVENT_EMBER_CB_RESET_MICRO_HANDLER) {
		if (mDriverState == INITIALIZING) {
			syslog(LOG_ERR, "Unexpected reset durring NCP initialization.");
			mNCPInstance->mFailureCount++;
			PT_INIT(&mSubPT);
		} else if (mDriverState == INITIALIZING_WAITING_FOR_RESET) {
			mDriverState = INITIALIZING;
		}
	}

	EH_BEGIN_SUB(&mSubPT);

	if (mNCPInstance->get_ncp_state() == UPGRADING) {
		int status;
		EH_WAIT_UNTIL(
			(mNCPInstance->get_upgrade_status() != EINPROGRESS)
		);

		status = mNCPInstance->get_upgrade_status();

		if (status == 0) {
			syslog(LOG_INFO, "Firmware Update Complete.");
		} else {
			syslog(LOG_ERR, "Firmware Update Failed with Error %d", status);
			mNCPInstance->mFailureCount++;

			if (mNCPInstance->mFailureCount > mNCPInstance->mFailureThreshold) {
				mNCPInstance->change_ncp_state(FAULT);
			}
		}
	}

	if (mNCPInstance->get_ncp_state() == FAULT) {
		EH_EXIT();
	}

	syslog(LOG_INFO, "Initializing NCP");

	mNCPInstance->change_ncp_state(UNINITIALIZED);

	mNCPInstance->set_ncp_power(true);

	mNCPInstance->clear_nonpermanent_global_addresses();

	mNCPInstance->mNCPVersionString = "";
	mIsGoingIntoDeepSleep = false;

	mDriverState = INITIALIZING_WAITING_FOR_RESET;

	if (mResetIsExpected) {
		EH_WAIT_UNTIL_WITH_TIMEOUT(NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT, mResetIsExpected == false);

		if (eh_did_timeout) {
			// mResetIsExpected has been set for too long and we
			// haven't gotten a reset yet. This can prevent us from sleeping.
			// by incrementing the failure count here we will cause
			// another reset to occur in the code below.
			mNCPInstance->mFailureCount++;
			mResetIsExpected = false;
			syslog(LOG_ERR, "Was waiting for a reset, but we never got one.");
		}
	} else {
		// Backoff delay. Normaly zero. May increase if we are in a reset loop.
		EH_SLEEP_FOR(mNCPInstance->mRunawayResetBackoffManager.delay_for_unexpected_reset());
	}

	do {
		mDriverState = INITIALIZING_WAITING_FOR_RESET;

		if (mNCPInstance->mFailureCount > mNCPInstance->mFailureThreshold) {
			syslog(LOG_ALERT, "The NCP is misbehaving: Repeatedly unable to initialize NCP. Entering fault state.");
			mNCPInstance->change_ncp_state(FAULT);
			EmberNCPControlInterface::reset_callbacks(-1);
			EH_EXIT();
		}

		if ( mNCPInstance->mAutoUpdateFirmware
		  && (mNCPInstance->mFailureCount > mNCPInstance->mFailureThreshold - 1)
		  && mNCPInstance->can_upgrade_firmware()
		) {
			syslog(LOG_ALERT, "The NCP is misbehaving: Attempting a firmware update");
			mNCPInstance->upgrade_firmware();
			EH_RESTART();
		}

		if (event == EVENT_EMBER_CB_RESET_MICRO_HANDLER) {
			// Wait until we haven't received a packet
			// from the NCP for a full second, so that
			// we make sure we have flushed out all
			// of the inbound NCP traffic.
			do {
				event = EVENT_IDLE;
				EH_WAIT_UNTIL_WITH_TIMEOUT(
					1, /* seconds */
					IS_EVENT_FROM_EMBER(event) || IS_EVENT_FROM_NCP(event)
				);
			} while(!eh_did_timeout);

		} else if (mNCPInstance->mFailureCount > 0) {
			syslog(LOG_ERR, "Resetting and trying again... (retry %d)", mNCPInstance->mFailureCount);

			mNCPInstance->change_ncp_state(UNINITIALIZED);

			mGotInitCallback = false;
			mNetworkKey = Data();
			mNetworkKeyIndex = 0;

			reset_callbacks(kWPANTUNDStatus_Canceled);

			// Wait until we haven't received a packet
			// from the NCP for a full second, so that
			// we make sure we have flushed out all
			// of the inbound NCP traffic.
			do {
				event = EVENT_IDLE;
				EH_WAIT_UNTIL_WITH_TIMEOUT(
					1, /* seconds */
					IS_EVENT_FROM_EMBER(event) || IS_EVENT_FROM_NCP(event)
				);
			} while(!eh_did_timeout);

			// Do a hard reset only on even attempts.
			if ((mNCPInstance->mFailureCount & 1) == 0) {
				mNCPInstance->hard_reset_ncp();
			} else {
				// Soft Reset
				emberResetMicro();

				CONTROL_REQUIRE_FLUSH_WITHIN(
					NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
					on_error
				);
			}

			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				event == EVENT_EMBER_CB_RESET_MICRO_HANDLER,
				on_error
			);
			mDriverState = INITIALIZING;
		}

		// Get the version, if necessary.
		if (mNCPInstance->mNCPVersionString.empty()) {
			emberGetVersions();

			CONTROL_REQUIRE_FLUSH_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				on_error
			);

			// Wait for the response
			EH_REQUIRE_WITHIN(
				(mNCPInstance->mFailureCount == 0)?NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT:2, // Make failure cases fail faster
				event == EVENT_EMBER_CB_GET_VERSIONS_RETURN
				|| event == EVENT_NCP_CONN_RESET,
				on_error
			);

			if (event == EVENT_NCP_CONN_RESET) {
				goto on_error;
			}
		}

		{
			emberInitializeHostAddressTable();
			emberInit();

			CONTROL_REQUIRE_FLUSH_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				on_error
			);

			// Wait for the response
			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				event == EVENT_EMBER_CB_INIT_RETURN,
				on_error
			);
		}

		if (mNCPInstance->get_ncp_state() == UNINITIALIZED) {
			emberState();

			CONTROL_REQUIRE_FLUSH_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				on_error
			);

			// Wait for the response
			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				mNCPInstance->get_ncp_state() != UNINITIALIZED,
				on_error
			);
		}

		// If we are "joining" at this point, then we must start over.
		// This will cause a reset to occur.
		require(!ncp_state_is_joining(mNCPInstance->get_ncp_state()), on_error);

		// This next line causes any resets received after this
		// point to cause the control protothread to be restarted.
		mDriverState = INITIALIZING;

		if (mNCPInstance->mEnabled) {
			if (mRequestedTXPower != USE_DEFAULT_TX_POWER) {
				emberSetRadioPower(std::min(mNCPTXPowerLimit,mRequestedTXPower));

				CONTROL_REQUIRE_FLUSH_WITHIN(
					NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
					on_error
				);

				// Wait for the response
				EH_REQUIRE_WITHIN(
					NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
					event == EVENT_EMBER_CB_SET_RADIO_POWER_RETURN,
					on_error
				);
			}

			if (mTXPowerMode != USE_DEFAULT_TX_POWER_MODE) {
				emberSetTxPowerMode(mTXPowerMode);

				CONTROL_REQUIRE_FLUSH_WITHIN(
					NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
					on_error
				);

				// Wait for the response
				EH_REQUIRE_WITHIN(
					NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
					event == EVENT_EMBER_CB_SET_TX_POWER_MODE_RETURN,
					on_error
				);
			}

			if (mCCAThreshold != USE_DEFAULT_CCA_THRESHOLD) {
				emberSetCcaThreshold(mCCAThreshold);

				CONTROL_REQUIRE_FLUSH_WITHIN(
					NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
					on_error
				);

				// Wait for the response
				EH_REQUIRE_WITHIN(
					NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
					event == EVENT_EMBER_CB_SET_CCA_THRESHOLD_RETURN,
					on_error
				);
			}

			if (mTransmitHookActive != USE_DEFAULT_TRANSMIT_HOOK_ACTIVE) {
				// Send the command
				nestEmberSetTransmitHookActive(mTransmitHookActive);
				CONTROL_REQUIRE_FLUSH_WITHIN(
					NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
					on_error
				);
			}
		}

		EH_REQUIRE_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			mNCPInstance->get_ncp_state() != UNINITIALIZED,
			on_error
		);

		if (ncp_state_has_joined(mNCPInstance->get_ncp_state())) {
			// Always exit initialization without permit join set.
			emberPermitJoining(0);

			CONTROL_REQUIRE_FLUSH_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				on_error
			);

			EH_WAIT_UNTIL_WITH_TIMEOUT(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				event == EVENT_EMBER_CB_PERMIT_JOINING_RETURN
			);

			// If we initialized into the joined state, then we must
			// have a network key. If not we should start over.
			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				!mNetworkKey.empty(),
				on_error
			);
		}

		break;

on_error:
		EH_SLEEP_FOR(0.5);
		mNCPInstance->mFailureCount++;
	} while (true);

	mNCPInstance->mFailureCount = 0;
	mResetIsExpected = false;
	mDriverState = NORMAL_OPERATION;

	syslog(LOG_NOTICE, "Finished initializing NCP");
	mCallbackStore.handle(EVENT_NCP_NOW_AWAKE, 0, NULL, 0);

	EH_END();
}

int
EmberNCPControlInterface::vprocess_event(int event, va_list args)
{
	if (mNCPInstance->get_ncp_state() == FAULT) {
		// We perform no processing in the fault state.
		PT_INIT(&mControlPT);
		return 0;
	}

	if (IS_EVENT_FROM_EMBER(event)) {
		//syslog(LOG_INFO, "EMBER EVENT 0x%04X",event);
		va_list tmp;
		va_copy(tmp, args);
		process_ember_callback(event, tmp);
		va_end(tmp);
	}

	while (!mTaskQueue.empty()) {
		va_list tmp;
		boost::shared_ptr<HelperTask> current_task(mTaskQueue.front());
		va_copy(tmp, args);
		char ret = current_task->vprocess_event(event, tmp);
		va_end(tmp);

		if (ret == PT_ENDED || ret == PT_EXITED) {
			mTaskQueue.pop_front();
			continue;
		}

		break;
	}

	EH_BEGIN();

	EH_SPAWN(&mSubPT, vprocess_init(event, args));

	if (mNCPInstance->get_ncp_state() == FAULT) {
		EH_EXIT();
	}

	EH_WAIT_UNTIL(mTaskQueue.empty());

	// If we are in the saved association state and we
	if (mNCPInstance->mAutoResume && mNCPInstance->mEnabled && (mNCPInstance->get_ncp_state() == COMMISSIONED)) {
		EH_SPAWN(&mSubPT, vprocess_saved(event, args));

		// The problem is that when the NCP is a router, we won't
		// automatically get sent the network key upon resuming
		// the network. We can only get the key by issuing another
		// CB_INIT_HOST command *after the network is resumed*.
		// This is an obvious bug, but this fix is pretty simple
		// and should have no adverse side effects.
		if (mNCPInstance->mNodeType != LURKER) {
			EH_WAIT_UNTIL_WITH_TIMEOUT(30, ((mNCPInstance->get_ncp_state() != ISOLATED) && (mNCPInstance->get_ncp_state() != COMMISSIONED)) || !mNCPInstance->mEnabled);

			if (mNCPInstance->mEnabled) {
				emberInitHost();
				CONTROL_REQUIRE_FLUSH_WITHIN(
					NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
					on_error
				);
			}
		}
	}

	while (1) {
		// Yield for one loop cycle only. This prevents
		// us from entering any endless loops.
		EH_SLEEP_FOR(0);

		if (ncp_state_is_initializing(mNCPInstance->get_ncp_state())) {
			EH_RESTART();

		} else if (!mNCPInstance->mEnabled) {
			syslog(LOG_NOTICE, "Interface Disabled.");
			EH_SPAWN(&mSubPT, vprocess_disabled(event, args));

		} else if (mMfgTestMode) {
			EH_SPAWN(&mSubPT, vprocess_mfg(event, args));

		} else if (mNCPInstance->get_ncp_state() == NET_WAKE_ASLEEP) {
			EH_SPAWN(&mSubPT, vprocess_duty_cycled_sleep(event, args));

		} else if (mNCPInstance->get_ncp_state() == NET_WAKE_WAKING) {
			EH_SPAWN(&mSubPT, vprocess_joined_net_wake(event, args));

		} else if (ncp_state_is_joining_or_joined(mNCPInstance->get_ncp_state())) {
			EH_SPAWN(&mSubPT, vprocess_tickle(event, args));

		} else if (!ncp_state_is_interface_up(mNCPInstance->get_ncp_state())) {
			EH_SPAWN(&mSubPT, vprocess_disconnected(event, args));

		} else {
			syslog(
				LOG_WARNING,
				"Unexpected association state %d (%s)",
				mNCPInstance->get_ncp_state(),
				ncp_state_to_string(mNCPInstance->get_ncp_state()).c_str()
			);

			// Yield for one main loop cycle without
			// specifying a timeout to avoid high CPU usage
			// cases like this.
			EH_YIELD();
		}

		continue;

on_error:
		syslog(LOG_ERR, "NCP is misbehaving or unresponsive");
		mNCPInstance->mFailureCount++;

		// Breaking out of this while loop will
		// cause us to restart this protothread.
		break;
	} // while(1)

	EH_END();
}

// ----------------------------------------------------------------------------
// MARK: -

cms_t
EmberNCPControlInterface::get_ms_to_next_event(void)
{
	cms_t cms = EventHandler::get_ms_to_next_event();

	if (ncp_state_is_detached_from_ncp(mNCPInstance->get_ncp_state())) {
		return CMS_DISTANT_FUTURE;
	}

	// If the control protothread hasn't even started, set cms to zero.
	if (0 == mControlPT.lc) {
		cms = 0;
	}

	if (!mTaskQueue.empty()) {
		int tmp_cms = mTaskQueue.front()->get_ms_to_next_event();
		if (tmp_cms < cms) {
			cms = tmp_cms;
		}
	}

	if (cms < 0) {
		cms = 0;
	}

	return cms;
}

int
EmberNCPControlInterface::process()
{
	int ret;

	mNCPInstance->mRunawayResetBackoffManager.update();

	ret = process_event(EVENT_IDLE);

	return ret;
}



// Helpful for use with callbacks.
int
EmberNCPControlInterface::process_event_helper(int event)
{
	return EventHandler::process_event(event);
}
