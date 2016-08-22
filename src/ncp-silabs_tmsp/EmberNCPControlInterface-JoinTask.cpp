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
 *		This file contains the code that handles joining a network.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include "EmberNCPControlInterface-JoinTask.h"
#include "EmberNCPUtils.h"
#include "string-utils.h"

using namespace nl;
using namespace nl::wpantund;

EmberNCPControlInterface::JoinTask::JoinTask(EmberNCPControlInterface* interface, CallbackWithStatusArg1 cb, const WPAN::NetworkInstance& network, NodeType node_type, const uint8_t prefix[8]):
	HelperTask(interface,cb), mNetwork(network), mNodeType(node_type)
{
	if (prefix) {
		memcpy(mPrefix, prefix, sizeof(mPrefix));
	} else {
		const uint8_t * xpanid = network.xpanid;

		// Default ML Prefix to be derived from XPANID.
		uint8_t mesh_local_prefix[8] = {
			0xfd, xpanid[0], xpanid[1], xpanid[2], xpanid[3], xpanid[4], 0, 0
		};

		memcpy(mPrefix, mesh_local_prefix, sizeof(mPrefix));
	}
}

int
EmberNCPControlInterface::JoinTask::vprocess_event(int event, va_list args)
{
	int ret = 0;
	EmberNetworkParameters parameters = {};
	uint16_t options = EMBER_NETWORK_ID_OPTION;
	uint32_t channel_mask = 0;

	EH_BEGIN();

	if (!mInterface->mNCPInstance->mEnabled) {
		ret = kWPANTUNDStatus_InvalidWhenDisabled;
	} else if (ncp_state_is_initializing(mInterface->mNCPInstance->get_ncp_state())) {
		ret = kWPANTUNDStatus_Busy;
	} else if (mInterface->mNCPInstance->is_initializing_ncp()) {
		ret = kWPANTUNDStatus_Busy;
	} else if (ncp_state_is_joining_or_joined(mInterface->mNCPInstance->get_ncp_state())) {
		ret = kWPANTUNDStatus_Already;
	} else if (ncp_state_is_commissioned(mInterface->mNCPInstance->get_ncp_state())) {
		ret = kWPANTUNDStatus_Already;
	}

	if (ret != 0) {
		finish(ret);
		EH_EXIT();
	}

	syslog(LOG_INFO,
		   "Joining \"%s\" xpanid:%016llX panid:%04X channel:%d",
		   mNetwork.name.c_str(),
		   static_cast<long long unsigned>(mNetwork.get_xpanid_as_uint64()),
		   mNetwork.panid,
		   mNetwork.channel);

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);

	if ((mInterface->mNetworkKey.size() == 16) && (mNetwork.get_xpanid_as_uint64() != 0)) {
		emberEnableHostJoinClient(false);

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			on_error
		);

		emberCommissionNetwork(
			mNetwork.channel,
			EMBER_ALL_802_15_4_CHANNELS_MASK,
			(const uint8_t*)mNetwork.name.c_str(),
			mNetwork.name.size(),
			mNetwork.panid,
			mPrefix,
			mNetwork.xpanid,
			(const EmberKeyData *)mInterface->mNetworkKey.data(),
			mInterface->mNetworkKeyIndex
		);

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			on_error
		);

		EH_REQUIRE_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			event == EVENT_EMBER_CB_COMMISSION_NETWORK_RETURN,
			on_error
		);

		ret = EmberGlue::status_to_wpantund_status(va_arg(args, int));

		require_noerr(ret, on_error);

		emberJoinCommissioned(
			mInterface->mTXPower,
			EmberGlue::node_type_convert_to_ember(mNodeType, mInterface->mNCPInstance->mLegacyInterfaceEnabled),
			false
		);

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			on_error
		);
	} else {
		emberEnableHostJoinClient(true);

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			on_error
		);

		strncpy((char*)parameters.networkId, mNetwork.name.c_str(), sizeof(parameters.networkId));

		if (mNodeType != UNKNOWN) {
			options |= EMBER_NODE_TYPE_OPTION;
			parameters.nodeType = EmberGlue::node_type_convert_to_ember(mNodeType, mInterface->mNCPInstance->mLegacyInterfaceEnabled);
		}

		parameters.panId = mNetwork.panid;

		if (mNetwork.panid != 0xFFFF) {
			options |= EMBER_PAN_ID_OPTION;
		}

		if (mNetwork.get_xpanid_as_uint64() != 0) {
			options |= EMBER_EXTENDED_PAN_ID_OPTION;
			memcpy(parameters.extendedPanId, mNetwork.xpanid, sizeof(parameters.extendedPanId));
		}

		if (mNetwork.channel != 0) {
			channel_mask = (1 << mNetwork.channel);
			parameters.channel = mNetwork.channel;
		}

		if ((abs(mInterface->mTXPower) & ~0x7F) == 0) {
			options |= EMBER_TX_POWER_OPTION;
			parameters.radioTxPower = mInterface->mTXPower;
		}

		emberJoinNetwork(&parameters, options, channel_mask);

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			on_error
		);
	}

	// We don't wait for this one because we
	// don't want to miss `EVENT_EMBER_CB_JOIN_NETWORK_RETURN`.
	// We send this so that we can get some insight
	// about the current state of the NCP during the
	// joining process.
	emberState();

	EH_REQUIRE_WITHIN(
		NCP_JOIN_TIMEOUT,
		event == EVENT_EMBER_CB_JOIN_NETWORK_RETURN
		|| event == EVENT_EMBER_CB_START_HOST_JOIN_CLIENT_HANDLER,
		on_error
	);

	if (event == EVENT_EMBER_CB_JOIN_NETWORK_RETURN) {
		ret = EmberGlue::status_to_wpantund_status(va_arg(args, int));

		if (ret == kWPANTUNDStatus_Ok) {
#if EMBER_BUILD_NUMBER >= 426
			do {
				EH_REQUIRE_WITHIN(
					NCP_JOIN_TIMEOUT,
					event == EVENT_EMBER_CB_NETWORK_STATUS_HANDLER ||
					event == EVENT_EMBER_CB_START_HOST_JOIN_CLIENT_HANDLER,
					on_error
				);

				if (event == EVENT_EMBER_CB_START_HOST_JOIN_CLIENT_HANDLER) {
					// Break from the while(true) loop, the event is then handled by the
					// next "if (event == EVENT_EMBER_CB_START_HOST_JOIN_CLIENT_HANDLER)" clause.
					break;
				}

				EmberNetworkStatus newNetworkStatus = va_arg_small(args, EmberNetworkStatus);
				EmberNetworkStatus oldNetworkStatus = va_arg_small(args, EmberNetworkStatus);

				(void)oldNetworkStatus;

				if (newNetworkStatus == EMBER_JOINED_NETWORK_ATTACHED) {
					ret = kWPANTUNDStatus_Ok;
					break; // out of while loop.
				} else {
#if EMBER_BUILD_NUMBER >= 509
					EmberJoinFailureReason reason = va_arg_small(args, EmberJoinFailureReason);
					ret = ember_join_failure_reason_to_wpantund_status(reason);
#elif EMBER_BUILD_NUMBER >= 426 && EMBER_BUILD_NUMBER < 509
					EmberNetworkStatusReason reason = va_arg_small(args, EmberNetworkStatusReason);
					ret = ember_network_status_reason_to_wpantund_status(reason);
#endif
					require(ret == kWPANTUNDStatus_Ok, on_error);
				}

				event = EVENT_IDLE;   // Clear event so we do not process it again in next loop.

			} while(true);

			// Fetch the state to ensure wpantund NCP state gets updated (no need to wait for callback).
			emberState();
#endif

		} else {
			// Fetch the state so that we are sure we are in the correct state.
			// Don't block, so that `ret` stays valid.
			emberState();
		}
	}

	if (event == EVENT_EMBER_CB_START_HOST_JOIN_CLIENT_HANDLER) {
		const uint8_t *parent_address = va_arg(args, const uint8_t*);

		mInterface->mNCPInstance->mNodeType = mNodeType;
		mInterface->mNCPInstance->mCurrentNetworkInstance = mNetwork;
		mInterface->mNCPInstance->change_ncp_state(CREDENTIALS_NEEDED);

		ret = kWPANTUNDStatus_InProgress;

		if (parent_address) {
			finish(ret, Data(parent_address, 8));
		} else {
			finish(ret);
		}
	}

	if (ret == kWPANTUNDStatus_Ok) {
		mInterface->mNCPInstance->mCurrentNetworkInstance = mNetwork;

		if (!ncp_state_has_joined(mInterface->mNCPInstance->get_ncp_state())) {
			// If we haven't already gotten a state change event
			// indicating that we are joined, ask for one.
			emberState();
			CONTROL_REQUIRE_FLUSH_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				on_error
			);
			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				event == EVENT_EMBER_CB_STATE_RETURN,
				on_error
			);
		}
	}

	finish(ret);

	EH_EXIT();

on_error:

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "Join failed: %d", ret);

	finish(ret);

	if (mInterface->mNCPInstance->get_ncp_state() != OFFLINE) {
		emberResetMicro();

		mInterface->mNCPInstance->change_ncp_state(UNINITIALIZED);

		EH_YIELD();
	}

	EH_END();
}
