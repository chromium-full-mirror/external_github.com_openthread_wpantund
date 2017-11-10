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
#include "FirmwareInterface.h"

#include "CallbackStore.hpp"
#include "config-file.h"
#include "nlpt.h"
#include "string-utils.h"
#include "any-to.h"
#include "time-utils.h"
#include "wpantund.h"

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

#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;

// -----------------------------

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
EmberNCPControlInterface::HelperTask::GetNCPCallbackReturnStatus(int event, va_list args)
{
	const uint8_t* data = NULL;
	size_t data_len = 0;
	va_list tmp;

	if (!IS_EVENT_FROM_NCP(event)) {
		return 0;
	}

	va_copy(tmp, args);
	data = va_arg(tmp, const uint8_t*);
	data_len = va_arg(tmp, size_t);
	va_end(tmp);

	return mInterface->mFirmwareInterface->GetReturnStatusFromNCPFrame(data, data_len);
}


// -----------------------------

EmberNCPControlInterface::EmberNCPControlInterface() :
	NCPControlInterface()
{
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
	mDefaultChannelMask = EmberCommands::FirmwareInterface::kAllChannelsMask;
	mUseLegacyChannel = true;
	mMfgTestMode = false;
	mMfgFirstPacketRSSI = 0;
	mMfgFirstPacketLQI = 0;
	mMfgRepeatRandomTXInterval = 0;
	mMfgRepeatRandomTXLen = 0;
	mNCPInstance = NULL;
	mManagementVersion = 0;
	mNetWakeState = 0;
	mDriverState = INITIALIZING_WAITING_FOR_RESET;
	mLastCommandSent = 0;
	mGotInitCallback = false;
	mNCPInitFlushTime = 1.0; // In Seconds
	mNCPInitResetWaitTime = 0.5; // In Seconds

	PT_INIT(&mSleepPT);
	PT_INIT(&mSubPT);


	// Initial firmware interface is the default firmware interface.
	mFirmwareInterface = EmberCommands::FirmwareInterface::GetFirmwareInterfaceForVersion();
}

int
EmberNCPControlInterface::StartNewTask(const boost::shared_ptr<HelperTask> &task)
{
	if (ncp_state_is_detached_from_ncp(mNCPInstance->get_ncp_state())) {
		task->finish(kWPANTUNDStatus_InvalidWhenDisabled);
	} else if (PT_SCHEDULE(task->process_event(EVENT_STARTING_TASK))) {

		if (ncp_state_is_sleeping(mNCPInstance->get_ncp_state())
			&& (dynamic_cast<const WakeupTask*>(task.get()) == NULL)
			&& !mIsWakingUpFromSleep
		) {
			StartNewTask(boost::shared_ptr<HelperTask>(new WakeupTask(this, NilReturn())));
		}
		mTaskQueue.push_back(task);
		return 0;
	}
	return -1;
}

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
		return;
	}

	if (options.count(kWPANTUNDProperty_NetworkName)) {
		network.name = any_to_string(options.at(kWPANTUNDProperty_NetworkName));
	}

	if (options.count(kWPANTUNDProperty_NetworkNodeType)) {
		node_type = string_to_node_type(any_to_string(options.at(kWPANTUNDProperty_NetworkNodeType)));
	}

	if (options.count(kWPANTUNDProperty_NCPChannel)) {
		network.channel = any_to_int(options.at(kWPANTUNDProperty_NCPChannel));
	}

	if (options.count(kWPANTUNDProperty_NetworkPANID)) {
		network.panid = any_to_int(options.at(kWPANTUNDProperty_NetworkPANID));
	}

	if (options.count(kWPANTUNDProperty_NetworkXPANID)) {
		network.set_xpanid_as_uint64(any_to_uint64(options.at(kWPANTUNDProperty_NetworkXPANID)));
	}

	if (options.count(kWPANTUNDProperty_IPv6MeshLocalPrefix)) {
		mesh_local_prefix = any_to_ipv6(options.at(kWPANTUNDProperty_IPv6MeshLocalPrefix));
	} else {
		memcpy(mesh_local_prefix.s6_addr, mNCPInstance->mNCPV6Prefix, sizeof(mNCPInstance->mNCPV6Prefix));
	}

	StartNewTask(boost::shared_ptr<HelperTask>(
		new JoinTask(
			this,
			boost::bind(cb,_1),
			network,
			node_type,
			(mesh_local_prefix.s6_addr[0]==0xFD)?mesh_local_prefix.s6_addr:NULL
		)
	));

	END_TRY_FOR_CMD(cb);
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

	StartNewTask(boost::shared_ptr<HelperTask>(
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
	StartNewTask(boost::shared_ptr<HelperTask>(
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
		StartNewTask(boost::shared_ptr<HelperTask>(
			new SendCommandTask(this, boost::bind(cb,_1), mFirmwareInterface->CMD_Resume(), mFirmwareInterface->GetIDFor_CBResume())
		));
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
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					boost::bind(cb,_1),
					mFirmwareInterface->CMD_Reset(),
					mFirmwareInterface->GetIDFor_CBReset()
				)
			));
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
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			mFirmwareInterface->CMD_StartWakeup(data),
			mFirmwareInterface->GetIDFor_CBStartWakeup()
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::host_did_wake(CallbackWithStatus cb)
{
	bool should_immediately_begin_low_power(false);

	if (((mNCPInstance->get_ncp_state() == DEEP_SLEEP) && mNCPInstance->can_set_ncp_power())
	 || (mNCPInstance->get_ncp_state() == FAULT)
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
		if (mNCPInstance->get_ncp_state() == DEEP_SLEEP) {
			wakeup();
		}
		cb(0);
		return;
	}

	if (ncp_state_is_sleeping(mNCPInstance->get_ncp_state())
		|| (mNCPInstance->get_ncp_state() == OFFLINE)
	) {
		should_immediately_begin_low_power = true;
	}

	if ((mNCPInstance->mNodeType == LURKER) && (mNCPInstance->get_ncp_state() == ASSOCIATED)) {
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

	if (ncp_state_is_sleeping(mNCPInstance->get_ncp_state())) {
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
		StartNewTask(boost::shared_ptr<HelperTask>(new DeepSleepTask(this, boost::bind(cb,_1))));
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
			StartNewTask(boost::shared_ptr<HelperTask>(new DeepSleepTask(this, boost::bind(cb,_1))));
		} else {
			switch(mNCPInstance->mNodeType) {
			case LURKER:
				BEGIN_TRY_FOR_CMD(cb);
				StartNewTask(boost::shared_ptr<HelperTask>(
					new LurkTask(
						this,
						boost::bind(cb,_1)
					)
				));
				END_TRY_FOR_CMD(cb);
				break;

			case SLEEPY_END_DEVICE:
				BEGIN_TRY_FOR_CMD(cb);
				StartNewTask(boost::shared_ptr<HelperTask>(
					new SendCommandTask(
						this,
						boost::bind(cb,_1),
						mFirmwareInterface->CMD_OKToNap(),
						mFirmwareInterface->GetIDFor_CBOKToNap()
					)
				));
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
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			mFirmwareInterface->CMD_GetState(),
			mFirmwareInterface->GetIDFor_CBState()
		)
	));
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
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			mFirmwareInterface->CMD_DataPoll(),
			// We will only get a callback if the NCP isn't
			// configured to automatically poll.
			(mSleepPollInterval == 0)
				? mFirmwareInterface->GetIDFor_CBDataPoll()
				: EVENT_NULL
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::peek(uint32_t address, uint16_t count, CallbackWithStatusArg1 cb)
{
	cb(kWPANTUNDStatus_FeatureNotSupported, std::string("Feature not supported"));
}

void
EmberNCPControlInterface::poke(uint32_t address, Data bytes, CallbackWithStatus cb)
{
	cb(kWPANTUNDStatus_FeatureNotSupported);
}

void
EmberNCPControlInterface::add_on_mesh_prefix(
	const struct in6_addr *prefix,
	bool defaultRoute,
	bool preferred,
	bool slaac,
	bool onMesh,
	OnMeshPrefixPriority priority,
	CallbackWithStatus cb
) {
	using namespace nl::wpantund::EmberCommands;
	uint8_t flags(0);
	struct in6_addr address = *prefix;

	require_action(prefix != NULL, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(mNCPInstance->mEnabled, bail, cb(kWPANTUNDStatus_InvalidWhenDisabled));

	BEGIN_TRY_FOR_CMD(cb);

	if (defaultRoute) {
		flags |= FirmwareInterface::DEFAULT_ROUTE_FLAG;
	}

	flags |= FirmwareInterface::SLAAC_PREFERRED_FLAG | FirmwareInterface::SLAAC_VALID_FLAG;

	if (!buffer_is_nonzero(prefix->s6_addr+8, 8)) {
		address = make_slaac_addr_from_eui64(prefix->s6_addr, mNCPInstance->mMACAddress);
	}

	config_gateway_address(
		flags,
		address,
		IPV6_MAX_LIFETIME,
		IPV6_MAX_LIFETIME,
		cb
	);

	END_TRY_FOR_CMD(cb);

bail:
	return;
}

void
EmberNCPControlInterface::remove_on_mesh_prefix(
	const struct in6_addr *prefix,
	CallbackWithStatus cb
) {
	using namespace nl::wpantund::EmberCommands;
	struct in6_addr address = *prefix;

	require_action(prefix != NULL, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(mNCPInstance->mEnabled, bail, cb(kWPANTUNDStatus_InvalidWhenDisabled));

	BEGIN_TRY_FOR_CMD(cb);

	if (!buffer_is_nonzero(prefix->s6_addr+8, 8)) {
		address = make_slaac_addr_from_eui64(prefix->s6_addr, mNCPInstance->mMACAddress);
	}

	config_gateway_address(
		0,
		address,
		0,
		0,
		cb
	);

	END_TRY_FOR_CMD(cb);

bail:
	return;
}

void
EmberNCPControlInterface::add_external_route(
	const struct in6_addr *prefix,
	int prefix_len_in_bits,
	int domain_id,
	ExternalRoutePriority priority,
	CallbackWithStatus cb
) {
	uint8_t flags = 0;

	require_action(prefix != NULL, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(prefix_len_in_bits >= 0, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(prefix_len_in_bits <= IPV6_MAX_PREFIX_LENGTH, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(mNCPInstance->mEnabled, bail, cb(kWPANTUNDStatus_InvalidWhenDisabled));

	switch (priority) {
		case ROUTE_HIGH_PREFERENCE:
			flags = EmberCommands::FirmwareInterface::HIGH_PREFERENCE;
			break;

		case ROUTE_LOW_PREFRENCE:
			flags = EmberCommands::FirmwareInterface::LOW_PREFERENCE;
			break;

		case ROUTE_MEDIUM_PREFERENCE:
			flags = 0;
			break;
	}

	syslog(LOG_INFO, "Adding external route with priority \"%s\"", external_route_priority_to_string(priority).c_str());

	config_external_route(flags, prefix->s6_addr, prefix_len_in_bits, domain_id, cb);

bail:
	return;
}

void
EmberNCPControlInterface::remove_external_route(
	const struct in6_addr *prefix,
	int prefix_len_in_bits,
	int domain_id,
	CallbackWithStatus cb
) {
	uint8_t flags = EmberCommands::FirmwareInterface::TEMPORARY_FLAG;

	require_action(prefix != NULL, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(prefix_len_in_bits >= 0, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(prefix_len_in_bits <= IPV6_MAX_PREFIX_LENGTH, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(mNCPInstance->mEnabled, bail, cb(kWPANTUNDStatus_InvalidWhenDisabled));

	syslog(LOG_INFO, "Removing external route");
	config_external_route(flags, prefix->s6_addr, prefix_len_in_bits, domain_id, cb);
bail:
	return;
}

void
EmberNCPControlInterface::joiner_add(
	const char *psk,
	uint32_t joiner_timeout,
	const uint8_t *addr,
	CallbackWithStatus cb
) {
	cb(kWPANTUNDStatus_FeatureNotImplemented);
}

void
EmberNCPControlInterface::config_gateway_address(uint8_t flags, const struct in6_addr &address,
	uint32_t preferredLifetime, uint32_t validLifetime, CallbackWithStatus cb)
{
	syslog(LOG_INFO, "Configure Gateway: flags:%s", mFirmwareInterface->configure_gateway_flags_to_string(flags).c_str());

	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			mFirmwareInterface->CMD_ConfigGateway(
				flags,
				address.s6_addr,
				sizeof(address),
				preferredLifetime,
				validLifetime,
				0 // domainID, just use the default for now.
			),
			mFirmwareInterface->GetIDFor_CBConfigGatewayReturn()
		)
	));
}

void
EmberNCPControlInterface::config_external_route(uint8_t flags, const uint8_t *route_prefix, int prefix_len_in_bits,
	uint8_t domain_id, CallbackWithStatus cb)
{
	char addr_cstr[INET6_ADDRSTRLEN] = "::";
	struct in6_addr addr;
	int prefix_len_in_bytes(IPV6_PREFIX_BITS_TO_BYTES(prefix_len_in_bits));

	require_action(route_prefix != NULL, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(IPV6_PREFIX_BYTES_TO_BITS(prefix_len_in_bytes) == prefix_len_in_bits, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(prefix_len_in_bits >= 0, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(prefix_len_in_bits <= IPV6_MAX_PREFIX_LENGTH, bail, cb(kWPANTUNDStatus_InvalidArgument));
	require_action(mNCPInstance->mEnabled, bail, cb(kWPANTUNDStatus_InvalidWhenDisabled));

	memset(addr.s6_addr, 0, sizeof(struct in6_addr));
	memcpy(addr.s6_addr, route_prefix, prefix_len_in_bytes);
	inet_ntop(AF_INET6, &addr, addr_cstr, sizeof(addr_cstr));
	syslog(LOG_INFO, "Configure External Route: Route prefix: \"%s\", prefix len: %d, domain id: %d, flags: %s (0x%02x)",
		addr_cstr, prefix_len_in_bytes, domain_id,
		mFirmwareInterface->configure_gateway_flags_to_string(flags).c_str(), flags
	);

	BEGIN_TRY_FOR_CMD(cb);

	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			mFirmwareInterface->CMD_ConfigExternalRoute(
					flags,
					route_prefix,
					prefix_len_in_bytes,
					domain_id
			),
			mFirmwareInterface->GetIDFor_CBConfigExternalRoute()
		)
	));

	END_TRY_FOR_CMD(cb);
bail:
	return;
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

	// Go ahead and set the passthru port.
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			NilReturn(),
			mFirmwareInterface->CMD_SetPassthruPort(mPassthruPort)
		)
	));

	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			mFirmwareInterface->CMD_PermitJoin(seconds, network_wide),
			mFirmwareInterface->GetIDFor_CBPermitJoinReturn()
		)
	));

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
	StartNewTask(boost::shared_ptr<HelperTask>(
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
		ret = send_management_command(mFirmwareInterface->CMD_StopScan());
	}
	END_TRY_FOR_CMD(cb);

	cb(ret);
}

void
EmberNCPControlInterface::netscan_start(const ValueMap& options, CallbackWithStatus cb)
{
	scan_start(kScanTypeNetScan, options, cb);
}

void
EmberNCPControlInterface::netscan_stop(CallbackWithStatus cb)
{
	scan_stop(cb);
}

void
EmberNCPControlInterface::energyscan_start(const ValueMap& options, CallbackWithStatus cb)
{
	scan_start(kScanTypeEnergyScan, options, cb);
}

void
EmberNCPControlInterface::energyscan_stop(CallbackWithStatus cb)
{
	scan_stop(cb);
}

struct GetCounterHelper {
	CallbackWithStatusArg1 cb;

	GetCounterHelper(const CallbackWithStatusArg1& _cb) : cb(_cb) { }

	void operator()(int status, const boost::any &value)const
	{
		uint16_t counter_value(0);

		if (status == kWPANTUNDStatus_Ok) {
			Data data = boost::any_cast<Data>(value);

			if (EmberCommands::UnpackArguments(data, "uv", NULL, &counter_value) < 0) {
				status = -errno;
			}
		}

		cb(status, boost::any(counter_value));
	}
};

void
EmberNCPControlInterface::get_counter(uint8_t index, CallbackWithStatusArg1 cb)
{
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			GetCounterHelper(cb),
			mFirmwareInterface->CMD_GetCounter(index),
			mFirmwareInterface->GetIDFor_CBCounters()
		)
	));
}

void
EmberNCPControlInterface::pcap_to_fd(int fd, CallbackWithStatus cb)
{
	cb(kWPANTUNDStatus_FeatureNotImplemented);
}

void
EmberNCPControlInterface::pcap_terminate(CallbackWithStatus cb)
{
	cb(kWPANTUNDStatus_FeatureNotImplemented);
}

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
		properties.insert(kWPANTUNDProperty_NetworkName);
		properties.insert(kWPANTUNDProperty_NetworkPANID);
		properties.insert(kWPANTUNDProperty_NetworkXPANID);
		properties.insert(kWPANTUNDProperty_NetworkKey);
		properties.insert(kWPANTUNDProperty_NetworkKeyIndex);
		properties.insert(kWPANTUNDProperty_NetworkNodeType);

		properties.insert(kWPANTUNDProperty_NCPState);
		properties.insert(kWPANTUNDProperty_NCPSleepyPollInterval);
		properties.insert(kWPANTUNDProperty_NCPTXPowerLimit);
		properties.insert(kWPANTUNDProperty_NCPChannelMask);

		properties.insert(kWPANTUNDProperty_IPv6MeshLocalAddress);
		properties.insert(kWPANTUNDProperty_IPv6LinkLocalAddress);
		properties.insert(kWPANTUNDProperty_IPv6AllAddresses);

		properties.insert(kWPANTUNDProperty_DaemonAutoAssociateAfterReset);
		properties.insert(kWPANTUNDProperty_DaemonAutoDeepSleep);
		properties.insert(kWPANTUNDProperty_DaemonEnabled);
		properties.insert(kWPANTUNDProperty_DaemonReadyForHostSleep);

		properties.insert(kWPANTUNDProperty_DebugControlProtothreadLine);

		properties.insert(kWPANTUNDProperty_NestLabs_LegacyMeshLocalAddress);
		properties.insert(kWPANTUNDProperty_NestLabs_LegacyMeshLocalPrefix);
		properties.insert(kWPANTUNDProperty_NestLabs_LegacyEnabled);
		properties.insert(kWPANTUNDProperty_NestLabs_NetworkAllowingJoin);
		properties.insert(kWPANTUNDProperty_NestLabs_NetworkPassthruPort);
		properties.insert(kWPANTUNDProperty_NestLabs_NetworkWakeData);
		properties.insert(kWPANTUNDProperty_NestLabs_NetworkWakeRemaining);
		properties.insert(kWPANTUNDProperty_NestLabs_HackUseDeepSleepOnLowPower);
		properties.insert(kWPANTUNDProperty_NestLabs_NetworkWakeBlacklist);
		properties.insert(kWPANTUNDProperty_NestLabs_HackAlwaysResetToWake);
		properties.insert(kWPANTUNDProperty_NestLabs_LegacyPreferInterface);
	}

	properties.insert(kWPANTUNDProperty_NestLabs_NCPTransmitHookActive);

	properties.insert(kWPANTUNDProperty_DaemonVersion);
	properties.insert(kWPANTUNDProperty_DaemonTerminateOnFault);

	properties.insert(kWPANTUNDProperty_NCPChannel);
	properties.insert(kWPANTUNDProperty_NCPVersion);
	properties.insert(kWPANTUNDProperty_NCPHardwareAddress);
	properties.insert(kWPANTUNDProperty_NCPCCAThreshold);
	properties.insert(kWPANTUNDProperty_NCPTXPower);

	properties.insert(kWPANTUNDProperty_NetworkIsCommissioned);

	properties.insert(kWPANTUNDProperty_SiLabs_NCPTXPowerMode);
	properties.insert(kWPANTUNDProperty_SiLabs_MfgTestMode);
	properties.insert(kWPANTUNDProperty_SiLabs_NCPInitFlushTime);
	properties.insert(kWPANTUNDProperty_SiLabs_NCPInitResetWaitTime);

	properties.insert(kWPANTUNDProperty_ConfigNCPDriverName);

	for(i=0; i < mFirmwareInterface->GetCounterCount(); i++) {
		properties.insert(kWPANTUNDProperty_SiLabs_CounterPrefix + mFirmwareInterface->CounterIndexToString(i));
	}

	return properties;
}

void
EmberNCPControlInterface::property_get_value(
    const std::string& in_key, CallbackWithStatusArg1 cb
    )
{
	std::string key = in_key;

	if (!mNCPInstance->is_initializing_ncp()) {
		syslog(LOG_INFO, "property_get_value: key: \"%s\"", in_key.c_str());
	}

	BEGIN_TRY_FOR_CMD(boost::bind(cb,_1,boost::any()));

	if (strcaseequal(key.c_str(), kWPANTUNDProperty_ConfigNCPDriverName)) {
		cb(0, boost::any(std::string("silabs")));

	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPCCAThreshold)) {
		if (!mNCPInstance->mEnabled) {
			cb(kWPANTUNDStatus_InvalidWhenDisabled, boost::any(std::string("Not available while disabled")));
		} else {
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					cb,
					mFirmwareInterface->CMD_GetCCAThreshold(),
					mFirmwareInterface->GetIDFor_CBGetCCAThreshold(),
					NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
					EmberCommands::PacketFieldType::EMBER_PACKED_INT8_CHAR_T
				)
			));
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_NCPTXPowerMode)) {
		if (!mNCPInstance->mEnabled) {
			cb(kWPANTUNDStatus_InvalidWhenDisabled, boost::any(std::string("Not available while disabled")));
		} else if (mMfgTestMode) {
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					cb,
					mFirmwareInterface->CMD_MfgGet(EmberCommands::FirmwareInterface::POWER_MODE),
					mFirmwareInterface->GetIDFor_CBMfgGetPowerMode(),
					NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
					EmberCommands::PacketFieldType::EMBER_PACKED_UINT16_CHAR_T
				)
			));
		} else {
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					cb,
					mFirmwareInterface->CMD_GetTXPowerMode(),
					mFirmwareInterface->GetIDFor_CBGetTXPowerMode(),
					NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
					EmberCommands::PacketFieldType::EMBER_PACKED_UINT16_CHAR_T
				)
			));
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPChannel)) {
		if (mMfgTestMode) {
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					cb,
					mFirmwareInterface->CMD_MfgGet(EmberCommands::FirmwareInterface::CHANNEL),
					mFirmwareInterface->GetIDFor_CBMfgGetChannel(),
					NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
					EmberCommands::PacketFieldType::EMBER_PACKED_INT8_CHAR_T
				)
			));
		} else {
			cb(0, boost::any(get_current_network_instance().channel));
		}
	} else if ((key == kWPANTUNDProperty_SiLabs_MfgSYNOffset) && mMfgTestMode) {
		StartNewTask(boost::shared_ptr<HelperTask>(
			new SendCommandTask(
				this,
				cb,
				mFirmwareInterface->CMD_MfgGet(EmberCommands::FirmwareInterface::SYN_OFFSET),
				mFirmwareInterface->GetIDFor_CBMfgGetSynOffset(),
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				EmberCommands::PacketFieldType::EMBER_PACKED_INT8_CHAR_T
			)
		));
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
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_NCPInitFlushTime)) {
		cb(0, boost::any(mNCPInitFlushTime));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_NCPInitResetWaitTime)) {
		cb(0, boost::any(mNCPInitResetWaitTime));
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
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					cb,
					mFirmwareInterface->CMD_MfgGet(EmberCommands::FirmwareInterface::POWER),
					mFirmwareInterface->GetIDFor_CBMfgGetPower(),
					NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
					EmberCommands::PacketFieldType::EMBER_PACKED_INT8_CHAR_T
				)
			));
		} else {
			cb(0, boost::any(static_cast<int>(mTXPower)));
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPTXPowerLimit)) {
		cb(0, boost::any(static_cast<int>(mNCPTXPowerLimit)));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkWakeBlacklist)) {
		cb(0, boost::any(mActiveWakeupBlacklist));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkKey)) {
		cb(0, boost::any(mNetworkKey));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPState)) {
		cb(0, boost::any(ncp_state_to_string(mNCPInstance->get_ncp_state())));
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
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_LegacyPreferInterface)) {
		cb(0, boost::any(mUseLegacyChannel));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkKeyIndex)) {
		cb(0, boost::any(mNetworkKeyIndex));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkPassthruPort)) {
		cb(0, boost::any(ntohs(mPassthruPort)));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_HackAlwaysResetToWake)) {
		cb(0, boost::any(mAlwaysResetToWake));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_DebugControlProtothreadLine)) {
		cb(0, boost::any(static_cast<int>(mControlPT.lc)));
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_IPv6RIPEntries)) {
		if (ncp_state_is_sleeping(mNCPInstance->get_ncp_state())) {
			cb(kWPANTUNDStatus_InvalidForCurrentState, boost::any(std::string("Will not fetch RIPEntries when NCP is asleep")));
		} else {
			StartNewTask(boost::shared_ptr<HelperTask>(new GetRIPEntryTask(this, cb)));
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_IPv6RIPEntriesValueMap)) {
		if (ncp_state_is_sleeping(mNCPInstance->get_ncp_state())) {
			cb(kWPANTUNDStatus_InvalidForCurrentState, boost::any(std::string("Will not fetch RIPEntries when NCP is asleep")));
		} else {
			StartNewTask(boost::shared_ptr<HelperTask>(new GetRIPEntryTask(this, cb, GetRIPEntryTask::kOutputModeValueMapList)));
		}
	} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_IPv6AllAddresses))  {
		StartNewTask(boost::shared_ptr<HelperTask>(new GetGlobalAddressesTask(this, cb)));

	} else if (strncaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_CounterPrefix, sizeof(kWPANTUNDProperty_SiLabs_CounterPrefix)-1)) {

		int type = mFirmwareInterface->StringToCounterIndex(std::string(key.begin()+sizeof(kWPANTUNDProperty_SiLabs_CounterPrefix)-1, key.end()));
		if (type >= 0) {
			if (!mNCPInstance->mEnabled) {
				cb(kWPANTUNDStatus_InvalidWhenDisabled, boost::any(std::string("Not available while disabled")));
			} else {
				get_counter(static_cast<uint8_t>(type), cb);
			}
		}
	} else {
		mNCPInstance->NCPInstanceBase::property_get_value(in_key, cb);
	}
	END_TRY_FOR_CMD(boost::bind(cb,_1,boost::any()));
}

void
EmberNCPControlInterface::property_set_value(
    const std::string&                      key,
    const boost::any&                       value,
    CallbackWithStatus      cb
    )
{
	syslog(LOG_INFO, "property_set_value: key: \"%s\"", key.c_str());

	// If we are disabled, then the only property we
	// are allowed to set is kWPANTUNDProperty_DaemonEnabled.
	if (!mNCPInstance->mEnabled && !strcaseequal(key.c_str(), kWPANTUNDProperty_DaemonEnabled)) {
		cb(kWPANTUNDStatus_InvalidWhenDisabled);
		return;
	}

	try {
		if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkKey)) {
			nl::Data net_key = any_to_data(value);

			StartNewTask(boost::shared_ptr<HelperTask>(
				new SetNetworkKeyTask(
					this,
					boost::bind(cb,_1),
					net_key,
					mNetworkKeyIndex
				)
			));
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NetworkNodeType)) {
			NodeType node_type = string_to_node_type(any_to_string(value));

			if (node_type != UNKNOWN)
			{
				uint8_t type = mFirmwareInterface->NodeTypeConvertToEmber(node_type, mNCPInstance->mLegacyInterfaceEnabled);

				StartNewTask(boost::shared_ptr<HelperTask>(
					new SendCommandTask(
						this,
						boost::bind(cb,_1),
						mFirmwareInterface->CMD_ChangeNodeType(type),
						mFirmwareInterface->GetIDFor_CBChangeNodeType()
					)
				));
			} else {
				cb(kWPANTUNDStatus_InvalidArgument);
			}

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
				StartNewTask(boost::shared_ptr<HelperTask>(
					new SendCommandTask(
						this,
						boost::bind(cb,_1),
						mFirmwareInterface->CMD_MfgSetPower(mTXPowerMode, mRequestedTXPower),
						mFirmwareInterface->GetIDFor_CBMfgSet()
					)
				));

			} else {
				if (value.empty() || ((abs(any_to_int(value)) & ~0x7F) != 0)) {
					// If the given value is larger than 127 or less than -128,
					// or not set at all, then we use the default TX power of
					// the NCP.
					mRequestedTXPower = USE_DEFAULT_TX_POWER;
					cb(0);
				} else {
					mRequestedTXPower = any_to_int(value);
					mRequestedTXPower = std::min(mRequestedTXPower, mFirmwareInterface->GetMaxTXPower());
					mRequestedTXPower = std::max(mRequestedTXPower, mFirmwareInterface->GetMinTXPower());

					mTXPower = std::min(mNCPTXPowerLimit, mRequestedTXPower);

					if ((mNCPInstance->get_ncp_state() != DEEP_SLEEP) && !ncp_state_is_initializing(mNCPInstance->get_ncp_state())) {
						StartNewTask(boost::shared_ptr<HelperTask>(
							new SendCommandTask(
								this,
								boost::bind(cb,_1),
								mFirmwareInterface->CMD_SetRadioPower(mTXPower),
								mFirmwareInterface->GetIDFor_CBSetRadioPower()
							)
						));
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
				StartNewTask(boost::shared_ptr<HelperTask>(
					new SendCommandTask(
						this,
						boost::bind(cb,_1),
						mFirmwareInterface->CMD_StackDataPoll(mSleepPollInterval),
						mFirmwareInterface->GetIDFor_CBStackDataPoll()
					)
				));
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
					StartNewTask(boost::shared_ptr<HelperTask>(
						new SendCommandTask(
							this,
							boost::bind(cb,_1),
							mFirmwareInterface->CMD_SetTransmitHookActive(any_to_bool(value)),
							mFirmwareInterface->GetIDFor_CBSetTransmitHookActive()
						)
					));
				} else {
					cb(0);
				}
			}
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPHardwareAddress)) {
			Data data;
			if (!value.empty()) {
				data = any_to_data(value);
			}
			if ((data.size() != 0) && (data.size() != 8)) {
				throw std::invalid_argument("Hardware address must be 8 bytes long");
			}
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					boost::bind(cb,_1),
					mFirmwareInterface->CMD_WriteNestMACAddr(data.size()==8?data.data():NULL),
					mFirmwareInterface->GetIDFor_CBWriteNestMACAddr()
				)
			));
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPCCAThreshold)) {
			if (value.empty()) {
				mCCAThreshold = USE_DEFAULT_CCA_THRESHOLD;
				cb(0);
			} else {
				mCCAThreshold = any_to_int(value);
				if ((mNCPInstance->get_ncp_state() != DEEP_SLEEP) && !ncp_state_is_initializing(mNCPInstance->get_ncp_state())) {
					StartNewTask(boost::shared_ptr<HelperTask>(
						new SendCommandTask(
							this,
							boost::bind(cb,_1),
							mFirmwareInterface->CMD_SetCCAThreshold(any_to_int(value)),
							mFirmwareInterface->GetIDFor_CBSetCCAThreshold()
						)
					));
				} else {
					cb(0);
				}
			}
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NCPChannelMask)) {
			uint32_t mask = (uint32_t)any_to_int(value);
			if (mask & ~EmberCommands::FirmwareInterface::kAllChannelsMask) {
				cb(kWPANTUNDStatus_InvalidRange);
			} else {
				if (mask == 0) {
					mask = EmberCommands::FirmwareInterface::kAllChannelsMask;
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
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_NCPInitFlushTime)) {
			mNCPInitFlushTime = any_to_float(value);
			cb(0);
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_NCPInitResetWaitTime)) {
			mNCPInitResetWaitTime = any_to_float(value);
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

		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_LegacyPreferInterface)) {
			mUseLegacyChannel = any_to_bool(value);
			cb(0);
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkWakeBlacklist) || (key == "ActiveWakeupMask")) {
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
				StartNewTask(boost::shared_ptr<HelperTask>(
					new SendCommandTask(
						this,
						boost::bind(cb,_1),
						mFirmwareInterface->CMD_SetRadioPower(mTXPower),
						mFirmwareInterface->GetIDFor_CBSetRadioPower()
					)
				));
			} else {
				cb(0);
			}
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_HackUseDeepSleepOnLowPower)) {
			mUseDeepSleepOnLowPower = any_to_bool(value);
			cb(0);
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_LegacyEnabled)) {
			mNCPInstance->mLegacyInterfaceEnabled = any_to_bool(value);
			cb(0);
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_NestLabs_NetworkPassthruPort)) {
			mPassthruPort = htons(any_to_int(value));
			if ((mNCPInstance->get_ncp_state() != DEEP_SLEEP) && !ncp_state_is_initializing(mNCPInstance->get_ncp_state())) {
				StartNewTask(boost::shared_ptr<HelperTask>(
					new SendCommandTask(
						this,
						NilReturn(),
						mFirmwareInterface->CMD_SetPassthruPort(mPassthruPort)
					)
				));
			}
			cb(0);
		} else if ((key == kWPANTUNDProperty_SiLabs_MfgSYNOffset) && mMfgTestMode) {
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					boost::bind(cb,_1),
					mFirmwareInterface->CMD_MfgSetSYNOffset((int8_t)any_to_int(value)),
					EVENT_IDLE // SYN_OFFSET doesn't give us a mFirmwareInterface->GetIDFor_CBMfgSet()
				)
			));
		} else if ((key == kWPANTUNDProperty_NCPChannel) && mMfgTestMode) {
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					boost::bind(cb,_1),
					mFirmwareInterface->CMD_MfgSetChannel((int8_t)any_to_int(value)),
					mFirmwareInterface->GetIDFor_CBMfgSet()
				)
			));
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_MfgTestMode)) {
			bool bool_value = any_to_bool(value);
			if (mMfgTestMode != bool_value) {
				mMfgTestMode = bool_value;

				if (mMfgTestMode) {
					StartNewTask(boost::shared_ptr<HelperTask>(
						new SendCommandTask(
							this,
							boost::bind(cb,_1),
							mFirmwareInterface->CMD_MfgStartTest(),
							mFirmwareInterface->GetIDFor_CBMfgStartTest()
						)
					));
				} else {
					StartNewTask(boost::shared_ptr<HelperTask>(
						new SendCommandTask(
							this,
							boost::bind(cb,_1),
							mFirmwareInterface->CMD_MfgEndTest(),
							mFirmwareInterface->GetIDFor_CBMfgEndTest()
						)
					));
				}

			} else {
				cb(0);
			}
		} else if (strcaseequal(key.c_str(), kWPANTUNDProperty_SiLabs_NCPTXPowerMode)) {
			if (mMfgTestMode) {
				mTXPowerMode = any_to_int(value);
				StartNewTask(boost::shared_ptr<HelperTask>(
					new SendCommandTask(
						this,
						boost::bind(cb,_1),
						mFirmwareInterface->CMD_MfgSetPower(mTXPowerMode, mRequestedTXPower),
						mFirmwareInterface->GetIDFor_CBMfgSet()
					)
				));
			} else {
				if (value.empty()) {
					mTXPowerMode = USE_DEFAULT_TX_POWER_MODE;
					cb(0);
				} else {
					mTXPowerMode = any_to_int(value);
					if ((mNCPInstance->get_ncp_state() != DEEP_SLEEP) && !ncp_state_is_initializing(mNCPInstance->get_ncp_state())) {
						StartNewTask(boost::shared_ptr<HelperTask>(
							new SendCommandTask(
								this,
								boost::bind(cb,_1),
								mFirmwareInterface->CMD_SetTXPowerMode(any_to_int(value)),
								mFirmwareInterface->GetIDFor_CBSetTXPowerMode()
							)
						));
					} else {
						cb(0);
					}
				}
			}
		} else {
			mNCPInstance->NCPInstanceBase::property_set_value(key, value, cb);
		}
	} catch (const boost::bad_any_cast &x) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR,"property_set_value: Bad type for property \"%s\" (%s)", key.c_str(), x.what());
		cb(kWPANTUNDStatus_InvalidArgument);
	} catch (EmberCommands::FirmwareInterface::not_implemented) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR,"property_set_value: Property \"%s\" not supported with this firmware", key.c_str());
		cb(kWPANTUNDStatus_FeatureNotSupported);
	} catch (const std::invalid_argument &x) {
		// We will get a bad_any_cast exception if the property is of
		// the wrong type.
		syslog(LOG_ERR,"property_set_value: Invalid argument for property \"%s\" (%s)", key.c_str(), x.what());
		cb(kWPANTUNDStatus_InvalidArgument);
	}
}

void
EmberNCPControlInterface::property_insert_value(
	const std::string& key,
	const boost::any& value,
	CallbackWithStatus cb
) {
	cb(kWPANTUNDStatus_FeatureNotImplemented);
}

void
EmberNCPControlInterface::property_remove_value(
	const std::string& key,
	const boost::any& value,
	CallbackWithStatus cb
) {
	cb(kWPANTUNDStatus_FeatureNotImplemented);
}

// MARK: -

std::string
EmberNCPControlInterface::get_name() {
	return mNCPInstance->get_name();
}

const WPAN::NetworkInstance&
EmberNCPControlInterface::get_current_network_instance()const
{
	return mNCPInstance->get_current_network_instance();
}


NCPInstance&
EmberNCPControlInterface::get_ncp_instance()
{
	return (*mNCPInstance);
}

void
EmberNCPControlInterface::handle_ncp_state_change(NCPState new_ncp_state, NCPState old_ncp_state)
{
	if (new_ncp_state == UPGRADING) {
		goto bail;
	}

	if (!ncp_state_is_interface_up(old_ncp_state)
		&& ncp_state_is_interface_up(new_ncp_state)
	) {
		StartNewTask(boost::shared_ptr<HelperTask>(
			new SendCommandTask(
				this,
				NilReturn(),
				mFirmwareInterface->CMD_SetPassthruPort(mPassthruPort)
			)
		));

		mNCPInstance->set_online(true);

		if ((SLEEPY_END_DEVICE == mNCPInstance->mNodeType)
			&& (mSleepPollInterval > 0)
		) {
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					NilReturn(),
					mFirmwareInterface->CMD_StackDataPoll(mSleepPollInterval),
					mFirmwareInterface->GetIDFor_CBStackDataPoll()
				)
			));
		}
	} else if (ncp_state_is_interface_up(old_ncp_state)
				&& (new_ncp_state == COMMISSIONED)
				&& mNCPInstance->mAutoResume
	) {
		// We don't bother handling this transition if autoresume is on.
		goto bail;
	}


	if ((!ncp_state_has_joined(old_ncp_state) || old_ncp_state==NET_WAKE_ASLEEP)
		&& ncp_state_has_joined(new_ncp_state)
		&& ncp_state_is_interface_up(new_ncp_state)
		&& mNCPInstance->mEnabled // Avoid starting new tasks when we are disabled.
	) {
		syslog(LOG_INFO, "Listening for wakeups and alarms");
		StartNewTask(boost::shared_ptr<HelperTask>(
			new SendCommandTask(
				this,
				NilReturn(),
				mFirmwareInterface->CMD_Listen(false),
				mFirmwareInterface->GetIDFor_CBListen()
			)
		));

		if (mFirmwareInterface->IsSupported_CMDSetActiveWakeup()) {
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					NilReturn(),
					mFirmwareInterface->CMD_SetActiveWakeup(false),
					mFirmwareInterface->GetIDFor_CBSetActiveWakeup()
				)
			));
		}

		if (mNCPInstance->mNodeType == SLEEPY_END_DEVICE) {
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					NilReturn(),
					mFirmwareInterface->CMD_StackDataPoll(mSleepPollInterval),
					mFirmwareInterface->GetIDFor_CBStackDataPoll()
				)
			));
		}
	}

	if (ncp_state_is_sleeping(old_ncp_state)) {
		mIsWakingUpFromSleep = false;
	}

bail:
	return;
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
EmberNCPControlInterface::handle_cb_set_address(const Data& data)
{
	struct in6_addr mesh_address;
	char address_string[INET6_ADDRSTRLEN] = "::";

	require_noerr(mFirmwareInterface->Parse_CBSetAddress(data, &mesh_address), bail);

	mNCPInstance->mNCPMeshLocalAddress = mesh_address;
	inet_ntop(
		AF_INET6,
		&mesh_address,
		address_string,
		sizeof(address_string)
	);
	syslog(LOG_NOTICE, "[NCP->] Mesh-local Address is \"%s\"\n", address_string);

	refresh_addresses();
bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_set_driver_address(const Data& data)
{
	struct in6_addr address;
	char address_cstr[INET6_ADDRSTRLEN] = "::";

	require_noerr(mFirmwareInterface->Parse_CBSetAddress(data, &address), bail);

	inet_ntop(
		AF_INET6,
		&address,
		address_cstr,
		sizeof(address_cstr)
	);

	syslog(LOG_NOTICE, "[NCP->] Driver Address is \"%s\"\n", address_cstr);

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_external_route_change(const Data& data)
{
	struct in6_addr boarder_router_prefix;
	char prefix_string[INET6_ADDRSTRLEN] = "::";
	uint8_t prefixLength;
	bool available;

	require(mFirmwareInterface->Parse_CBExternalRouteChange(data,
				&boarder_router_prefix,
				&prefixLength,
				&available) != -1, bail);

	inet_ntop(
		AF_INET6,
		&boarder_router_prefix,
		prefix_string,
		sizeof(prefix_string)
	);

	prefixLength *= 8;	// Convert bytes to bits

	if (available == true) {
		syslog(LOG_NOTICE, "[NCP->] Available Border Router %s/%u\n",
			prefix_string,
			prefixLength);
		mNCPInstance->mPrimaryInterface->add_route(&boarder_router_prefix, prefixLength, 256);
	} else {
		syslog(LOG_NOTICE, "[NCP->] Not Available Border Router %s/%u\n",
			prefix_string,
			prefixLength);
		mNCPInstance->mPrimaryInterface->remove_route(&boarder_router_prefix, prefixLength, 256);
	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_dhcp_server_change(const Data& data)
{
	const uint8_t *prefix(NULL);
	int prefix_len(0);
	uint8_t available(0);
	struct in6_addr address = {};

	require(
		mFirmwareInterface->Parse_CBDHCPServerChange(
			data,
			&prefix,
			&prefix_len,
			&available
		) >= 0,
		bail
	);

	require(prefix_len >= 0, bail);
	require(prefix_len < 16, bail);

	memcpy(address.s6_addr, prefix, prefix_len);

	{
		char prefix_cstr[INET6_ADDRSTRLEN] = "::";
		inet_ntop(
			AF_INET6,
			&address,
			prefix_cstr,
			sizeof(prefix_cstr)
		);

		syslog(LOG_NOTICE, "[NCP->] DHCP Server for prefix \"%s/%u\" is %savailable", prefix_cstr, prefix_len * 8, available?"":"NOT ");
	}

	if (available && (prefix_len == 8)) {
		request_address_from_ncp(address, prefix_len * 8, GA_DHCP);
	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_request_dhcp_address(const Data& data)
{
	int status = 0;
	struct in6_addr dhcp_address;
	char address_string[INET6_ADDRSTRLEN] = "::";

	status = mFirmwareInterface->GetReturnStatusFromNCPFrame(data.data(), data.size());

	if (status != 0) {
		syslog(LOG_NOTICE, "[NCP->] DHCP Address request error: 0x%02X (%d)\n", status, status);
	} else {
		require(mFirmwareInterface->Parse_CBRequestDHCPAddress(data, &dhcp_address) != -1, bail);

		inet_ntop(
			AF_INET6,
			&dhcp_address,
			address_string,
			sizeof(address_string)
		);
		syslog(LOG_NOTICE, "[NCP->] Got DHCP Address from server \"%s\"\n", address_string);
	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_slaac_server_change(const Data& data)
{
	const uint8_t *prefix(NULL);
	int prefix_len(0);
	uint8_t available(0);
	struct in6_addr address = {};

	require(
		mFirmwareInterface->Parse_CBSLAACServerChange(
			data,
			&prefix,
			&prefix_len,
			&available
		) >= 0,
		bail
	);

	require(prefix_len >= 0, bail);
	require(prefix_len < 16, bail);

	memcpy(address.s6_addr, prefix, prefix_len);

	{
		char prefix_cstr[INET6_ADDRSTRLEN] = "::";
		inet_ntop(
			AF_INET6,
			&address,
			prefix_cstr,
			sizeof(prefix_cstr)
		);

		syslog(LOG_NOTICE, "[NCP->] SLAAC Server for prefix \"%s/%u\" is %savailable", prefix_cstr, prefix_len * 8, available?"":"NOT ");
	}

	if (available
	    && (prefix_len == 8)
		&& !mNCPInstance->lookup_address_for_prefix(NULL, address, 64)
	) {
		address = make_slaac_addr_from_eui64(prefix, mNCPInstance->mMACAddress);
		request_address_from_ncp(address, 128, 0);
	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_address_configuration_change(const Data& data)
{
	char address_string[INET6_ADDRSTRLEN] = "::";
	struct in6_addr address = {};
	uint32_t valid_lifetime(0), preferred_lifetime(0);
	uint8_t flags(0);

	require(
		mFirmwareInterface->Parse_CBAddressConfigurationChange(
			data,
			&address,
			&valid_lifetime,
			&preferred_lifetime,
			&flags
		) >= 0,
		bail
	);

	mNCPInstance->unicast_address_was_added(EmberNCPInstance::kOriginThreadNCP, address, 64, valid_lifetime, preferred_lifetime);

	inet_ntop(
		AF_INET6,
		&address,
		address_string,
		sizeof(address_string)
	);

	syslog(LOG_NOTICE, "[NCP->] Address Config Change: \"%s\" valid:%d preferred:%d flags:%s(0x%02X)\n", address_string, valid_lifetime, preferred_lifetime, address_flags_to_string(flags).c_str(),flags);

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_get_global_address(const Data& data)
{
	struct in6_addr address = {};
	uint32_t valid_lifetime(0), preferred_lifetime(0);
	uint8_t flags(0);

	require(
		mFirmwareInterface->Parse_CBGetGlobalAddress(
			data,
			&address,
			&preferred_lifetime,
			&valid_lifetime,
			&flags
		) >= 0,
		bail
	);

	// NOTE: An NCP bug can cause the returned valid_lifetime and preferred_lifetime values
	// to be zero even though the address is still valid. As a hack/fix for now, we check
	// for the case of zero and modify the values. (TODO: File a tracker/jira for this).
	if (valid_lifetime == 0) {
		valid_lifetime = 0xFFFFFFFF;
	}

	if (preferred_lifetime == 0) {
		preferred_lifetime = 0xFFFFFFFF;
	}

	mNCPInstance->unicast_address_was_added(EmberNCPInstance::kOriginThreadNCP, address, 64, valid_lifetime, preferred_lifetime);

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_permit_join(const Data& data)
{
	require(
		EmberCommands::UnpackArguments(
			data,
			EMBER_PACKED_UINT16 "u",
			NULL, // Skip command identifier
			&mNCPInstance->mCurrentNetworkInstance.joinable
		) != -1,
		bail
	);

	syslog(LOG_NOTICE, "Legacy Joining is %spermitted", mNCPInstance->mCurrentNetworkInstance.joinable?"":"NOT ");

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_listen(const Data& data)
{
	uint8_t result = 0;

	require(
		EmberCommands::UnpackArguments(
			data,
			EMBER_PACKED_UINT16 "u",
			NULL, // Skip command identifier
			&result
		) != -1,
		bail
	);

	if (result == 0) {
		mNetWakeState = EmberCommands::FirmwareInterface::WAKEUP_STATE_LISTENING;
	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_wakeup(const Data& data)
{
	uint8_t reason = 0;
	uint8_t state = 0;
	uint16_t ms_remaining = 0;
	uint8_t dataByte = 0;
	uint16_t ota_sequence = 0;

	require(
		EmberCommands::UnpackArguments(
			data,
			EMBER_PACKED_UINT16 "uuvuv",
			NULL, // Skip command id
			&reason,
			&state,
			&ms_remaining,
			&dataByte,
			&ota_sequence
		) != -1,
		bail
	);

	mNetWakeState = state;

	if (state == EmberCommands::FirmwareInterface::WAKEUP_STATE_TRANSMITTING) {
		cms_t remaining = ms_remaining;
		mNetWakeData = dataByte;
		mNetWakeEnd = time_ms() + remaining;

		syslog(LOG_NOTICE,"WAKEUP: In-progress, %dms remaining, data: 0x%02X (%d)",remaining, mNetWakeData, mNetWakeData);
		if ((0 != remaining)
		 && (mNCPInstance->get_ncp_state() != NET_WAKE_WAKING)
		) {
			mNCPInstance->change_ncp_state(NET_WAKE_WAKING);

			if (mFirmwareInterface->IsSupported_CMDSetActiveWakeup()) {
				// Unless this specific wake code is masked out,
				// turn on active wakeup.
				if (mActiveWakeupBlacklist.count(mNetWakeData) == 0) {
					syslog(LOG_INFO,"WAKEUP: Wakeup reason %d not masked, I WILL PROPAGATE!", mNetWakeData);
					StartNewTask(boost::shared_ptr<HelperTask>(
						new SendCommandTask(
							this,
							NilReturn(),
							mFirmwareInterface->CMD_SetActiveWakeup(true),
							mFirmwareInterface->GetIDFor_CBSetActiveWakeup()
						)
					));
				} else {
					syslog(LOG_INFO,"WAKEUP: Wakeup reason %d IS masked, I will NOT propagate.", mNetWakeData);
				}
			}
		}
		mOnNetWake(mNetWakeData, remaining);
	} else {
		syslog(LOG_NOTICE,"WAKEUP: Idle and %slistening, data: 0x%02X (%d)",
			state == EmberCommands::FirmwareInterface::WAKEUP_STATE_NOT_LISTENING ? "NOT " : "",
			mNetWakeData, mNetWakeData
		);
		if (mNCPInstance->get_ncp_state() == NET_WAKE_WAKING) {
			if (mFirmwareInterface->IsSupported_CMDSetActiveWakeup()) {
				StartNewTask(boost::shared_ptr<HelperTask>(
					new SendCommandTask(
						this,
						NilReturn(),
						mFirmwareInterface->CMD_SetActiveWakeup(false),
						mFirmwareInterface->GetIDFor_CBSetActiveWakeup()
					)
				));
			}

			// Indicate that the wake is over.
			mOnNetWake(mNetWakeData, 0);

			mNCPInstance->change_ncp_state(ASSOCIATED);
		}
	}

bail:
	return;
}

void
EmberNCPControlInterface::refresh_addresses(void)
{
	struct in6_addr legacy_addr;
	bool has_legacy_address = false;

	if (buffer_is_nonzero(mNCPInstance->mNCPV6LegacyPrefix, sizeof(mNCPInstance->mNCPV6LegacyPrefix))) {
		legacy_addr = make_slaac_addr_from_eui64(mNCPInstance->mNCPV6LegacyPrefix, mNCPInstance->mMACAddress);
		has_legacy_address = true;
	} else {
		memset(legacy_addr.s6_addr, 0, sizeof(legacy_addr));
	}

	if (!buffer_is_nonzero(mNCPInstance->mNCPV6Prefix, 8) && !buffer_is_nonzero(mNCPInstance->mNCPV6LegacyPrefix, 8)) {
		return;
	}

	if (memcmp(mNCPInstance->mNCPV6LegacyPrefix, mNCPInstance->mNCPV6Prefix, sizeof(mNCPInstance->mNCPV6Prefix)) == 0) {
		// If the prefixes are the same, then we can only assign the prefix
		// to one of the network interfaces.
		if (mNCPInstance->is_legacy_interface_enabled() && mUseLegacyChannel) {
			mNCPInstance->mLegacyInterface->add_address(&legacy_addr);
			mNCPInstance->unicast_address_was_removed(EmberNCPInstance::kOriginThreadNCP, mNCPInstance->mNCPMeshLocalAddress);
			memset(mNCPInstance->mNCPMeshLocalAddress.s6_addr, 0, sizeof(mNCPInstance->mNCPMeshLocalAddress));
		} else {
			if (static_cast<bool>(mNCPInstance->mLegacyInterface)) {
				mNCPInstance->mLegacyInterface->remove_address(&legacy_addr);
			}
			if (buffer_is_nonzero(&mNCPInstance->mNCPMeshLocalAddress.s6_addr[8], 8)) {
				mNCPInstance->unicast_address_was_added(EmberNCPInstance::kOriginThreadNCP, mNCPInstance->mNCPMeshLocalAddress);
			}
			memset(mNCPInstance->mNCPV6LegacyPrefix, 0, sizeof(mNCPInstance->mNCPV6LegacyPrefix));
		}
	} else {
		if (has_legacy_address && mNCPInstance->is_legacy_interface_enabled()) {
			mNCPInstance->mLegacyInterface->add_address(&legacy_addr);
		}

		if (buffer_is_nonzero(&mNCPInstance->mNCPMeshLocalAddress.s6_addr[8], 8)) {
			mNCPInstance->unicast_address_was_added(EmberNCPInstance::kOriginThreadNCP, mNCPInstance->mNCPMeshLocalAddress);
		}
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
EmberNCPControlInterface::handle_cb_state(const Data& data)
{
	static const uint8_t link_local_prefix[8] = {0xfe, 0x80};
	struct in6_addr new_ll_addr;
	int status;
	WPAN::NetworkInstance new_network_instance;
	int8_t tx_power = 0;
	uint8_t association_state = 0;
	uint8_t raw_type = 0;
	NCPState new_ncp_state(mNCPInstance->get_ncp_state());
	NodeType node_type;
	uint8_t mac_address[8];


	status = mFirmwareInterface->Parse_CBState(
		data,
		&new_network_instance,
		&tx_power,
		&raw_type,
		&association_state,
		mac_address,
		mNCPInstance->mNCPV6Prefix,
		mNCPInstance->mNCPV6LegacyPrefix
	);

	require(status >= 0, bail);

	mNCPInstance->mNodeTypeSupportsLegacy = EMBER_TYPE_SUPPORTS_LEGACY(raw_type);
	node_type = mFirmwareInterface->NodeTypeConvertFromEmber(raw_type);
	if (node_type != mNCPInstance->mNodeType) {
		mOnPropertyChanged(
			kWPANTUNDProperty_NetworkNodeType,
			node_type_to_string(mFirmwareInterface->NodeTypeConvertFromEmber(raw_type))
		);
	}
	mNCPInstance->mNodeType = node_type;

	if (mNCPInstance->mNodeType == LURKER) {
		// Lurkers by definition support a legacy interface.
		mNCPInstance->mLegacyInterfaceEnabled = true;
	}

	syslog(
		LOG_INFO,
		"NCP Status: State=%d, CH=%d, TXP=%d, raw_node_type=0x%02X",
		association_state,
		new_network_instance.channel,
		tx_power,
		raw_type
	);

	syslog(
		LOG_INFO,
		"NCP Status: MACAddr: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
		mNCPInstance->mMACAddress[0],mNCPInstance->mMACAddress[1],mNCPInstance->mMACAddress[2],mNCPInstance->mMACAddress[3],
		mNCPInstance->mMACAddress[4],mNCPInstance->mMACAddress[5],mNCPInstance->mMACAddress[6],mNCPInstance->mMACAddress[7]
	);

	syslog(
		LOG_INFO,
		"NCP Status: Mesh-Local Prefix: %02X%02X:%02X%02X:%02X%02X:%02X%02X::/64",
		mNCPInstance->mNCPV6Prefix[0],mNCPInstance->mNCPV6Prefix[1],mNCPInstance->mNCPV6Prefix[2],mNCPInstance->mNCPV6Prefix[3],
		mNCPInstance->mNCPV6Prefix[4],mNCPInstance->mNCPV6Prefix[5],mNCPInstance->mNCPV6Prefix[6],mNCPInstance->mNCPV6Prefix[7]
	);

	if (mNCPInstance->mNodeTypeSupportsLegacy) {
		syslog(
			LOG_INFO,
			"NCP Status: Legacy Prefix: %02X%02X:%02X%02X:%02X%02X:%02X%02X::/64",
			mNCPInstance->mNCPV6LegacyPrefix[0],mNCPInstance->mNCPV6LegacyPrefix[1],mNCPInstance->mNCPV6LegacyPrefix[2],mNCPInstance->mNCPV6LegacyPrefix[3],
			mNCPInstance->mNCPV6LegacyPrefix[4],mNCPInstance->mNCPV6LegacyPrefix[5],mNCPInstance->mNCPV6LegacyPrefix[6],mNCPInstance->mNCPV6LegacyPrefix[7]
		);
		mNCPInstance->enable_legacy_interface();
	}

	mNCPInstance->set_mac_address(mac_address);

	new_ll_addr = make_slaac_addr_from_eui64(link_local_prefix, mac_address);

	if (new_ll_addr != mNCPInstance->mNCPLinkLocalAddress) {
		mNCPInstance->unicast_address_was_removed(EmberNCPInstance::kOriginThreadNCP, mNCPInstance->mNCPLinkLocalAddress);
		mNCPInstance->mNCPLinkLocalAddress = new_ll_addr;
		mNCPInstance->unicast_address_was_added(EmberNCPInstance::kOriginThreadNCP, mNCPInstance->mNCPLinkLocalAddress);
	}

	// determine whether to carry over the old joinable state over or not
	new_network_instance.joinable = mNCPInstance->mCurrentNetworkInstance.joinable
		&& (static_cast<const WPAN::NetworkId &>(new_network_instance) == mNCPInstance->mCurrentNetworkInstance);

	switch (mFirmwareInterface->NCPStateConvertFromEmber(association_state)) {
	case ASSOCIATING:
		if (mNCPInstance->get_ncp_state() != CREDENTIALS_NEEDED) {
			new_ncp_state = ASSOCIATING;
		}
		break;

	case ASSOCIATED:
		if (mNCPInstance->get_ncp_state() != NET_WAKE_WAKING) {
			new_ncp_state = ASSOCIATED;
		}
		break;

	case ISOLATED:
		if (mNCPInstance->get_ncp_state() != NET_WAKE_WAKING) {
			new_ncp_state = ISOLATED;
		}
		break;

	case FAULT:
		assert_printf("Unknown NCP state %d", association_state);
		break;

	default:
		new_ncp_state = mFirmwareInterface->NCPStateConvertFromEmber(association_state);
		break;
	}

	if (new_ncp_state != CREDENTIALS_NEEDED) {
		mNCPInstance->mCurrentNetworkInstance = new_network_instance;
	}

	mNCPInstance->change_ncp_state(new_ncp_state);

	refresh_addresses();

	mTXPower = tx_power;

	// Actively limit transmit power
	if ((tx_power > mNCPTXPowerLimit)
		&& mNCPInstance->mEnabled
		&& !mIsGoingIntoDeepSleep
		&& (mDriverState == NORMAL_OPERATION)
	) {
		mTXPower = mNCPTXPowerLimit;
		syslog(LOG_NOTICE, "Limiting TXPower to %ddBm (was %ddBm)", mTXPower, tx_power);
		StartNewTask(boost::shared_ptr<HelperTask>(
			new SendCommandTask(
				this,
				NilReturn(),
				mFirmwareInterface->CMD_SetRadioPower(mTXPower),
				mFirmwareInterface->GetIDFor_CBSetRadioPower()
			)
		));
	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_set_network_keys(const Data& data)
{
	uint32_t network_key_index;
	Data network_key;

	require(
		mFirmwareInterface->Parse_CBSetNetworkKeys(
			data,
			&network_key_index,
			&network_key
		) != -1,
		bail
	);

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

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_get_versions(const Data& data)
{
	int management_version = mFirmwareInterface->GetManagementVersion();
	std::string version_string = mFirmwareInterface->GetVersionString();

	require(
		mFirmwareInterface->Parse_CBGetVersions(
			data,
			&management_version,
			&version_string
		) != -1,
		bail
	);

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

	try {
		mFirmwareInterface = EmberCommands::FirmwareInterface::GetFirmwareInterfaceForVersion(
			management_version,
			version_string
		);
	} catch (EmberCommands::FirmwareInterface::unsupported_firmware_version x) {
		syslog(LOG_ERR, "NCP FIRMWARE VERSION IS UNSUPPORTED");
		mNCPInstance->change_ncp_state(FAULT);
	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_cb_assert(const Data& data)
{
	int filename_size = 0;
	const char* filename_data = NULL;
	uint32_t line_number = 0;

	if (EmberCommands::UnpackArguments(
		data.data(),
		data.size(),
		EMBER_PACKED_UINT16 "bw",
		NULL,
		&filename_size,
		&filename_data,
		&line_number
	) == -1) {
		assert_printf("FAILED TO UNPACK ARGUMENTS (errno: %d, %s)", errno, strerror(errno));
		return;
	}

	syslog(LOG_ERR,"[NCP->] ASSERT %.*s:%d", filename_size, filename_data, line_number);
}

void
EmberNCPControlInterface::handle_cb_reset(const Data& data)
{
	// A reset occured.
	uint8_t reason = 0;

	if (EmberCommands::UnpackArguments(
		data.data(),
		data.size(),
		EMBER_PACKED_UINT16 "u",
		NULL,
		&reason
	) == -1) {
		assert_printf("FAILED TO UNPACK ARGUMENTS (errno: %d, %s)", errno, strerror(errno));
		return;
	}

	const char* reason_cstr = mFirmwareInterface->reset_cause_to_cstr(static_cast<EmberCommands::FirmwareInterface::resetCause>(reason));
	int severity = LOG_NOTICE;
	switch(static_cast<EmberCommands::FirmwareInterface::resetCause>(reason)) {
	case EmberCommands::FirmwareInterface::RESET_FIB:
	case EmberCommands::FirmwareInterface::RESET_BOOTLOADER:
	case EmberCommands::FirmwareInterface::RESET_EXTERNAL:
	case EmberCommands::FirmwareInterface::RESET_POWERON:
	case EmberCommands::FirmwareInterface::RESET_SOFTWARE:
		severity = LOG_NOTICE;
		break;

	case EmberCommands::FirmwareInterface::RESET_FLASH:
		severity = LOG_WARNING;
		break;

	default:
		severity = LOG_ERR;
		break;
	}
	syslog(severity, "[NCP->] RESET (Reason %d, \"%s\")", reason, reason_cstr);

	mGotInitCallback = false;

	// Cached network key is always cleared upon reset.
	mNetworkKey = Data();
	mNetworkKeyIndex = 0;
	memset(mNCPInstance->mNCPV6LegacyPrefix, 0, sizeof(mNCPInstance->mNCPV6LegacyPrefix));
	memset(mNCPInstance->mNCPV6Prefix, 0, sizeof(mNCPInstance->mNCPV6Prefix));

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
EmberNCPControlInterface::handle_ncp_event(int command_identifier, const uint8_t* data, size_t len)
{
	// We always handle the following events.
	if (command_identifier == mFirmwareInterface->GetIDFor_CBAssertInfo()) {
		handle_cb_assert(Data(data, len));
	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBReset()) {
		handle_cb_reset(Data(data, len));
	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBGetVersions()) {
		handle_cb_get_versions(Data(data, len));
	}

	if (mNCPInstance->mNCPVersionString.empty()) {
		// Until we have the version from the NCP,
		// we are not allowed to handle any other callbacks.
		goto bail;
	}

	if (mResetIsExpected
		&& (command_identifier != mFirmwareInterface->GetIDFor_CBEcho())
		&& (command_identifier != mFirmwareInterface->GetIDFor_CBState())
		&& (command_identifier != mFirmwareInterface->GetIDFor_CBWakeup())
	) {
		// We ignore any other command identifiers while an
		// internal reset is pending.
		goto bail;
	}

	if (!mNCPInstance->mEnabled
		&& !( (mNCPInstance->get_ncp_state() == UNINITIALIZED)
			&& (command_identifier == mFirmwareInterface->GetIDFor_CBState())
		)
		&& (command_identifier != mFirmwareInterface->GetIDFor_CBWakeup())
	) {
		// We ignore pretty much everything if we are disabled and initialized.
		// We make an exception for the state callback during initialization
		// because it is required for initialization to complete successfully.
		goto bail;
	}

	mCallbackStore.handle(
		command_identifier,
		mFirmwareInterface->GetReturnStatusFromNCPFrame(data, len),
		data,
		len
	);

	if (command_identifier == mFirmwareInterface->GetIDFor_CBSetAddress()) {
		handle_cb_set_address(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBSetDriverAddress()) {
		handle_cb_set_driver_address(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBExternalRouteChange()) {
		handle_cb_external_route_change(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBDHCPServerChange()) {
		handle_cb_dhcp_server_change(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBRequestDHCPAddress()) {
		handle_cb_request_dhcp_address(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBPermitJoin()) {
		handle_cb_permit_join(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBMfgRX()) {
		handle_cb_mfg_rx(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBListen()) {
		handle_cb_listen(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBWakeup()) {
		handle_cb_wakeup(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBState()) {
		handle_cb_state(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBSetNetworkKeys()) {
		handle_cb_set_network_keys(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBAddressConfigurationChange()) {
		handle_cb_address_configuration_change(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBSLAACServerChange()) {
		handle_cb_slaac_server_change(Data(data, len));

	} else if (command_identifier == mFirmwareInterface->GetIDFor_CBGetGlobalAddress()) {
		handle_cb_get_global_address(Data(data, len));

	}

bail:
	return;
}

void
EmberNCPControlInterface::handle_ncp_frame(
    uint8_t type, const uint8_t* data, size_t len
    )
{
	uint16_t command_identifier(0);

	if (type == MGNT_MSG_CMD) {
		syslog(LOG_INFO,
		       "[NCP->] Callback: %s length: %d\n",
			   mFirmwareInterface->DescribeCommandOrCallback(data, len).c_str(),
		       (int)len);

		require_noerr_action(mFirmwareInterface->UnwrapCustomCallback(data, len), bail, reinitialize_ncp());

		command_identifier = mFirmwareInterface->GetCMDID(data, len);
	}

	// If this method is called, we are no longer asleep.
	// Go ahead and change states.
	if ((mNCPInstance->get_ncp_state() == DEEP_SLEEP)
		&& !mIsGoingIntoDeepSleep
		&& !mResetIsExpected

		// Getting a CB_LISTEN when we think we are in a deep sleep state is OK.
		&& !((type == MGNT_MSG_CMD) && (command_identifier == mFirmwareInterface->GetIDFor_CBListen()))
	) {
		syslog(LOG_NOTICE, "NCP has awoken from deep sleep");
		mNCPInstance->change_ncp_state(UNINITIALIZED);
	} else if ((mNCPInstance->get_ncp_state() == NET_WAKE_ASLEEP)
		// Getting a CB_LISTEN when we think we are lurking is OK.
		&& !((type == MGNT_MSG_CMD) && (command_identifier == mFirmwareInterface->GetIDFor_CBListen()))
	) {
		syslog(LOG_NOTICE, "NCP has awoken");
		mNCPInstance->change_ncp_state(ASSOCIATED);
	}

	if (type == MGNT_MSG_CMD) {
#if DEBUG
		{
			char readable_buffer[MAX_PACKET_SIZE];
			encode_data_into_string(data, len, readable_buffer,
			                        sizeof(readable_buffer), 0);
			syslog(LOG_DEBUG|LOG_AUTHPRIV, "\t↳ %s", (const char*)readable_buffer);
		}
#endif

		handle_ncp_event(command_identifier, data, len);

		process_event(command_identifier, data, len);

	} else if (type == MGNT_MSG_STDERR) {
		static char linebuffer[400 + 1];
		static int linepos = 0;
		while (len--) {
			char nextchar = *data++;

			if (nextchar == 0)
				nextchar = '#';

			if ((nextchar != '\n') && (nextchar != '\r'))
				linebuffer[linepos++] = nextchar;

			if (linepos && (
			        nextchar == '\n'
			        || nextchar == '\r'
			        || (linepos >= (sizeof(linebuffer) - 1))
			        )
			    ) {
				// flush.
				linebuffer[linepos] = 0;
				syslog(LOG_INFO, "NCP => %s\n", linebuffer);
				linepos = 0;
			}
		}
	} else {
		char readable_buffer[MAX_PACKET_SIZE];
		int i;
		for (i = 0; i < len; ++i) {
			readable_buffer[i] = isprint(data[i]) ? data[i] : '.';
		}
		readable_buffer[i] = 0;
		syslog(LOG_INFO,
		       "[NCP->] MGNT %02x: [%s]",
		       type,
		       (const char*)readable_buffer);
	}

bail:
	return;
}


void
EmberNCPControlInterface::request_address_from_ncp(const struct in6_addr& addr, int prefix_len, int flags)
{
	try {
		int len_in_bytes(IPV6_PREFIX_BITS_TO_BYTES(prefix_len));

		if ((flags & GA_DHCP) == GA_DHCP) {
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					NilReturn(),
					mFirmwareInterface->CMD_RequestDHCPAddress(addr.s6_addr, len_in_bytes),
					mFirmwareInterface->GetIDFor_CBRequestDHCPAddress()
				)
			));
		} else {
			StartNewTask(boost::shared_ptr<HelperTask>(
				new SendCommandTask(
					this,
					NilReturn(),
					mFirmwareInterface->CMD_RequestSLAACAddress(addr.s6_addr, len_in_bytes),
					mFirmwareInterface->GetIDFor_CBRequestSLAACAddress()
				)
			));
		}
	} catch (EmberCommands::FirmwareInterface::not_implemented) {
	}
}

void
EmberNCPControlInterface::request_manual_address_from_ncp(const struct in6_addr& addr, int prefix_len)
{
	using namespace nl::wpantund::EmberCommands;
	if (is_router()) {
		const uint32_t kForeverLifetime = 0xFFFFFFFF;
		// Router roles always register a prefix by configuring a gateway
		config_gateway_address(FirmwareInterface::SLAAC_PREFERRED_FLAG | FirmwareInterface::SLAAC_VALID_FLAG,
			addr, kForeverLifetime, kForeverLifetime, NilReturn());
	} else {
		// non-Router roles do the usual thing
		request_address_from_ncp(addr, 128, 0);
	}
}


void
EmberNCPControlInterface::abandon_address_from_ncp(const struct in6_addr& addr, int prefix_len)
{
	try {
		StartNewTask(boost::shared_ptr<HelperTask>(
			new SendCommandTask(
				this,
				NilReturn(),
				mFirmwareInterface->CMD_ResignGlobalAddress(&addr),
				mFirmwareInterface->GetIDFor_CBResignGlobalAddress()
			)
		));
	} catch (EmberCommands::FirmwareInterface::not_implemented) {
	}
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
	mNCPInstance->reinitialize_ncp();
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
		StartNewTask(boost::shared_ptr<HelperTask>(
			new WakeupTask(
				this,
				boost::bind(cb,_1)
			)
		));
	}

	return ret;
}

int
EmberNCPControlInterface::send_management_command(const Data& data, CallbackWithStatus cb)
{
	if (ncp_state_is_detached_from_ncp(mNCPInstance->get_ncp_state())) {
		return -1;
	}

	// If this function returns NON-ZERO, then the callback WAS NOT CALLED.

	if (ncp_state_is_sleeping(mNCPInstance->get_ncp_state())) {
		mNCPInstance->set_ncp_power(true);
	}

	syslog(LOG_DEBUG, "Will send command %s...", mFirmwareInterface->DescribeCommandOrCallback(data.data(), data.size()).c_str());

	return mNCPInstance->send_management_command(mFirmwareInterface->WrapCustomCommand(data), cb);
}

// MARK: -

int
EmberNCPControlInterface::vprocess_disabled(int event, va_list args)
{
	EH_BEGIN_SUB(&mSubPT);

	while(!mNCPInstance->mEnabled) {
		// If the association state is uninitialized, fail early.
		if (mFirmwareInterface->GetVersionString().empty() || mIsWakingUpFromSleep) {
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

		if (mNCPInstance->get_ncp_state() != DEEP_SLEEP) {
			StartNewTask(boost::shared_ptr<HelperTask>(new DeepSleepTask(this, NilReturn())));

			EH_WAIT_UNTIL_WITH_TIMEOUT(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				(mNCPInstance->get_ncp_state() == DEEP_SLEEP) || mTaskQueue.empty()
			);
		}

		// If we didn't enter deep sleep then we need to bail early.
		if (mNCPInstance->get_ncp_state() != DEEP_SLEEP) {
			if (!ncp_state_is_initializing(mNCPInstance->get_ncp_state())) {
				// We only need to reset if the NCP isn't UNINITIALIZED.
				IGNORE_RETURN_VALUE(send_management_command(mFirmwareInterface->CMD_Reset()));
			}
			EH_EXIT();
		}

		mCallbackStore.handle(EVENT_NCP_DISABLED, 0, NULL, 0);

		EH_WAIT_UNTIL(!IS_EVENT_FROM_NCP(event));

		EH_REQUIRE_WITHIN(
			NCP_DEEP_SLEEP_TICKLE_TIMEOUT,
			(mNCPInstance->get_ncp_state() != DEEP_SLEEP)
			|| mNCPInstance->mEnabled
			|| IS_EVENT_FROM_NCP(event),
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
		CONTROL_REQUIRE_SENT_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			mFirmwareInterface->CMD_Reset(),
			timeout_error
		);

		mResetIsExpected = true;

		// Wait for the response.
		EH_REQUIRE_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			event == mFirmwareInterface->GetIDFor_CBReset(),
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
	StartNewTask(boost::shared_ptr<HelperTask>(new DeepSleepTask(this, NilReturn())));

	EH_WAIT_UNTIL_WITH_TIMEOUT(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		!mIsGoingIntoDeepSleep
	);

	// If we didn't enter deep sleep then we need to bail early.
	if (mNCPInstance->get_ncp_state() != DEEP_SLEEP) {
		if (!ncp_state_is_initializing(mNCPInstance->get_ncp_state())) {
			// We only need to reset if the NCP isn't UNINITIALIZED.
			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				mFirmwareInterface->CMD_Reset(),
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
		|| IS_EVENT_FROM_NCP(event),
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
		IS_EVENT_FROM_NCP(event),
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
	mNCPInstance->send_management_command(mFirmwareInterface->CMD_Reset());
	EH_EXIT();

timeout_error:
	syslog(LOG_WARNING, "NCP Timeout error");
	EH_EXIT();

	EH_END();
}

int
EmberNCPControlInterface::vprocess_joined_net_wake(int event, va_list args)
{
	// Conditions under which this protothread should be exited gracefully.
	const bool should_exit = !mNCPInstance->mEnabled || mMfgTestMode;

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

		CONTROL_REQUIRE_SENT_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			mFirmwareInterface->CMD_Listen(false),
			on_error
		);
	}

	EH_EXIT();

on_error:

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

	CONTROL_REQUIRE_SENT_WITHIN(
		NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
		mFirmwareInterface->CMD_Resume(),
		on_error
	);

	EH_WAIT_UNTIL_WITH_TIMEOUT(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		should_exit
		|| (event == mFirmwareInterface->GetIDFor_CBResume())
		|| ncp_state_has_joined(mNCPInstance->get_ncp_state())
	);

	if (eh_did_timeout) {
		CONTROL_REQUIRE_SENT_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			mFirmwareInterface->CMD_GetState(),
			on_error
		);
	}

	EH_EXIT();

on_error:

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
		|| IS_EVENT_FROM_NCP(event)
		|| ncp_state_is_sleeping(mNCPInstance->get_ncp_state())
	);

	if (eh_did_timeout) {
		StartNewTask(boost::shared_ptr<HelperTask>(new DeepSleepTask(this, NilReturn())));
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
		|| mMfgTestMode;

	EH_BEGIN_SUB(&mSubPT);

	EH_WAIT_UNTIL_WITH_TIMEOUT(
		NCP_TICKLE_TIMEOUT,
		should_exit || !IS_EVENT_FROM_NCP(event)
	);

	EH_WAIT_UNTIL_WITH_TIMEOUT(
		NCP_TICKLE_TIMEOUT,
		should_exit || IS_EVENT_FROM_NCP(event)
	);

	if (eh_did_timeout) {
		syslog(LOG_INFO, "Tickle...");
		CONTROL_REQUIRE_SENT_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			mFirmwareInterface->CMD_GetState(),
			on_error
		);
		EH_REQUIRE_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			should_exit || (event == mFirmwareInterface->GetIDFor_CBState()),
			on_error
		);
		mNCPInstance->mFailureCount = 0;
	}

	EH_EXIT();

on_error:

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

	reinitialize_ncp();

	EH_END();
}

int
EmberNCPControlInterface::vprocess_init(int event, va_list args)
{
	if (event == mFirmwareInterface->GetIDFor_CBInitHost()) {
		mGotInitCallback = true;
	}

	if (event == mFirmwareInterface->GetIDFor_CBReset()) {
		if (mDriverState == INITIALIZING) {
			syslog(LOG_ERR, "Unexpected reset during NCP initialization.");
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
			syslog(LOG_NOTICE, "Firmware Update Complete.");
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

	mNCPInstance->set_initializing_ncp(true);

	mNCPInstance->change_ncp_state(UNINITIALIZED);

	mNCPInstance->set_ncp_power(true);

	mNCPInstance->remove_all_address_prefix_route_entries();

	mNCPInstance->mNCPVersionString = "";
	mFirmwareInterface = EmberCommands::FirmwareInterface::GetFirmwareInterfaceForVersion();
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

		// Wait a little bit and see if we get a reset.
		EH_WAIT_UNTIL_WITH_TIMEOUT(
			mNCPInitResetWaitTime,
			mDriverState != INITIALIZING_WAITING_FOR_RESET
		);
	}

	do {
		if (mNCPInstance->mFailureCount > mNCPInstance->mFailureThreshold) {
			syslog(LOG_ALERT, "The NCP is misbehaving: Repeatedly unable to initialize NCP. Entering fault state.");
			mNCPInstance->change_ncp_state(FAULT);
			EmberNCPControlInterface::reset_callbacks(kWPANTUNDStatus_Canceled);
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

		if (event == mFirmwareInterface->GetIDFor_CBReset()) {
			// Wait until we haven't received a packet
			// from the NCP for mNCPInitFlushTime, so that
			// we make sure we have flushed out all
			// of the inbound NCP traffic.
			do {
				event = EVENT_IDLE;
				EH_WAIT_UNTIL_WITH_TIMEOUT(
					mNCPInitFlushTime,
					IS_EVENT_FROM_NCP(event)
				);
			} while(!eh_did_timeout);

		} else if ( (mDriverState == INITIALIZING_WAITING_FOR_RESET)
		         && (mNCPInstance->mFailureCount > 0)
		) {
			syslog(LOG_ERR, "Resetting and trying again... (retry %d)", mNCPInstance->mFailureCount);

			mNCPInstance->change_ncp_state(UNINITIALIZED);

			mGotInitCallback = false;
			mNetworkKey = Data();
			mNetworkKeyIndex = 0;

			// Wait until we haven't received a packet
			// from the NCP for mNCPInitFlushTime, so that
			// we make sure we have flushed out all
			// of the inbound NCP traffic.
			do {
				event = EVENT_IDLE;
				EH_WAIT_UNTIL_WITH_TIMEOUT(
					mNCPInitFlushTime,
					IS_EVENT_FROM_NCP(event)
				);
			} while(!eh_did_timeout);

			reset_callbacks(kWPANTUNDStatus_Canceled);

			// Do a hard reset only on even attempts.
			if ((mNCPInstance->mFailureCount & 1) == 0) {
				mNCPInstance->hard_reset_ncp();
			} else {
				// Soft Reset
				CONTROL_REQUIRE_SENT_WITHIN(
					NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
					mFirmwareInterface->CMD_Reset(),
					on_error
				);
			}

			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				event == mFirmwareInterface->GetIDFor_CBReset(),
				on_error
			);
			mDriverState = INITIALIZING;
		}

		// Get the version, if necessary.
		if (mNCPInstance->mNCPVersionString.empty()) {
			// Send the command
			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				mFirmwareInterface->CMD_GetVersions(),
				on_error
			);

			// Wait for the response
			EH_REQUIRE_WITHIN(
				(mNCPInstance->mFailureCount == 0)?NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT:2, // Make failure cases fail faster
				event == mFirmwareInterface->GetIDFor_CBGetVersions()
				|| event == EVENT_NCP_CONN_RESET,
				on_error
			);

			if (event == EVENT_NCP_CONN_RESET) {
				goto on_error;
			}
		}

		if (!mGotInitCallback) {
			// Send the command
			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				mFirmwareInterface->CMD_InitHost(),
				on_error
			);

			// Wait for the response
			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				event == mFirmwareInterface->GetIDFor_CBInitHost(),
				on_error
			);
		}

		if (mNCPInstance->get_ncp_state() == UNINITIALIZED) {
			// Send the command
			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				mFirmwareInterface->CMD_GetState(),
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
				// Send the command
				CONTROL_REQUIRE_SENT_WITHIN(
					NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
					mFirmwareInterface->CMD_SetRadioPower(std::min(mNCPTXPowerLimit,mRequestedTXPower)),
					on_error
				);

				// Wait for the response
				EH_REQUIRE_WITHIN(
					NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
					event == mFirmwareInterface->GetIDFor_CBSetRadioPower(),
					on_error
				);
			}

			if (mTXPowerMode != USE_DEFAULT_TX_POWER_MODE) {
				// Send the command
				CONTROL_REQUIRE_SENT_WITHIN(
					NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
					mFirmwareInterface->CMD_SetTXPowerMode(mTXPowerMode),
					on_error
				);

				// Wait for the response
				EH_REQUIRE_WITHIN(
					NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
					event == mFirmwareInterface->GetIDFor_CBSetTXPowerMode(),
					on_error
				);
			}

			if (mCCAThreshold != USE_DEFAULT_CCA_THRESHOLD) {
				// Send the command
				CONTROL_REQUIRE_SENT_WITHIN(
					NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
					mFirmwareInterface->CMD_SetCCAThreshold(mCCAThreshold),
					on_error
				);

				// Wait for the response
				EH_REQUIRE_WITHIN(
					NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
					event == mFirmwareInterface->GetIDFor_CBSetCCAThreshold(),
					on_error
				);
			}

			if (mTransmitHookActive != USE_DEFAULT_TRANSMIT_HOOK_ACTIVE) {
				// Send the command
				CONTROL_REQUIRE_SENT_WITHIN(
					NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
					mFirmwareInterface->CMD_SetTransmitHookActive((bool)mTransmitHookActive),
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
			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				mFirmwareInterface->CMD_PermitJoin(0, false),
				on_error
			);

			EH_WAIT_UNTIL_WITH_TIMEOUT(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				event == mFirmwareInterface->GetIDFor_CBPermitJoinReturn()
			);

			// If we initialized into the joined state, then we must
			// have a network key. If not we should start over.
			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				!mNetworkKey.empty(),
				on_error
			);

			// Ask NCP for any IPv6 addresses it may already have.
			// Callbacks to this command (if any) are processed by handle_ncp_event().
			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				mFirmwareInterface->CMD_GetGlobalAddresses(NULL, 0),
				on_error
			);
		}

		break;

on_error:
		EH_SLEEP_FOR(0.5);
		mNCPInstance->mFailureCount++;
		mNCPInstance->change_ncp_state(UNINITIALIZED);
		mDriverState = INITIALIZING_WAITING_FOR_RESET;
	} while (true);

	mNCPInstance->set_initializing_ncp(false);
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

	while (!mTaskQueue.empty()) {
		boost::shared_ptr<HelperTask> current_task(mTaskQueue.front());
		char ret = current_task->vprocess_event(event, args);

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
				CONTROL_REQUIRE_SENT_WITHIN(
					NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
					mFirmwareInterface->CMD_InitHost(),
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


// Helpful for use with callbacks.
int
EmberNCPControlInterface::process_event_helper(int event)
{
	return EventHandler::process_event(event);
}
