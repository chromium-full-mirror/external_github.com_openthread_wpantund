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
#include "EmberNCPControlInterface-SetNetworkKeyTask.h"
#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;

	;

EmberNCPControlInterface::SetNetworkKeyTask::SetNetworkKeyTask(EmberNCPControlInterface* interface, CallbackWithStatusArg1 cb, const nl::Data& network_key, uint32_t network_key_index):
	HelperTask(interface,cb), mNetworkKey(network_key), mNetworkKeyIndex(network_key_index)
{
	if (mNetworkKeyIndex == 0) {
		mNetworkKeyIndex = 1;
	}
}

int
EmberNCPControlInterface::SetNetworkKeyTask::vprocess_event(int event, va_list args)
{
	int ret = kWPANTUNDStatus_Timeout;

	EH_BEGIN();

	if (!mInterface->mNCPInstance->mEnabled) {
		ret = kWPANTUNDStatus_InvalidWhenDisabled;
	} else if (mNetworkKey.size() != 16) {
		ret = kWPANTUNDStatus_InvalidArgument;
		syslog(LOG_NOTICE, "Invalid key size of %d. Should be 16.", (int)mNetworkKey.size());
	} else {
		ret = kWPANTUNDStatus_Ok;
	}

	if (ret != 0) {
		finish(ret);
		EH_EXIT();
	}

	mNodeType = mInterface->mNCPInstance->mNodeType;
	mNetwork = mInterface->mNCPInstance->get_current_network_instance();

	if (buffer_is_nonzero(mInterface->mNCPInstance->mNCPV6Prefix, 8)) {
		memcpy(mPrefix, mInterface->mNCPInstance->mNCPV6Prefix, 8);
	} else {
		memset(mPrefix, 0, sizeof(mPrefix));
		memcpy(mPrefix+1, mNetwork.xpanid, 5);
		mPrefix[0] = 0xFD;
	}

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);

	// If we are in the UNINITIALIZED state, wait for us to be initialized first.
	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		(!ncp_state_is_initializing(mInterface->mNCPInstance->get_ncp_state())),
		on_error
	);

	if ((mInterface->mNCPInstance->get_ncp_state() == DEEP_SLEEP)
		|| (mInterface->mNCPInstance->get_ncp_state() == OFFLINE)
	) {
		if ((mInterface->mNetworkKey == mNetworkKey) && (mInterface->mNetworkKeyIndex == mNetworkKeyIndex)) {
			// The network key is already set to what we
			// expect it to be. In this case, we just return success.
			finish(kWPANTUNDStatus_Ok);
			EH_EXIT();
		}

		if (!mInterface->mNetworkKey.empty()) {
			syslog(LOG_NOTICE, "Network key has already been set. An NCP reset is required.");

			emberResetMicro();

			CONTROL_REQUIRE_FLUSH_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				on_error
			);

			mInterface->mNCPInstance->change_ncp_state(UNINITIALIZED);

			// Set the `mResetIsExpected` flag so that this
			// task isn't cleared out when the reset callback
			// is received.
			mInterface->mResetIsExpected = true;

			// Wait for the reset and re-initialization to complete.
			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				(!ncp_state_is_initializing(mInterface->mNCPInstance->get_ncp_state()))
				&& (mInterface->mDriverState == NORMAL_OPERATION),
				on_error
			);

			// For readability, go ahead and set this flag
			// to false here. This isn't really necessary,
			// since the flag is automatically reset upon
			// receipt of the reset callback, but it helps
			// with code clarity to leave it in.
			mInterface->mResetIsExpected = false;
		}

		syslog(LOG_NOTICE, "set_property: Setting network key using \"SetSecurityParams\"");

		{
			EmberSecurityParameters parameters = {};
			parameters.networkKey = reinterpret_cast<EmberKeyData*>(mNetworkKey.data());
			emberSetSecurityParameters(&parameters, EMBER_NETWORK_KEY_OPTION);
		}

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			on_error
		);

		// Wait for the response from the NCP.
		EH_REQUIRE_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			event == EVENT_EMBER_CB_SET_SECURITY_PARAMETERS_RETURN,
			on_error
		);

		// Interpret the return value from the response.
		ret = EmberGlue::status_to_wpantund_status(va_arg(args, int));


	} else if (mInterface->mNCPInstance->get_ncp_state() == CREDENTIALS_NEEDED) {
		if (mInterface->mNCPInstance->mNodeType != LURKER) {
			emberResetMicro();

			CONTROL_REQUIRE_FLUSH_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				on_error
			);

			mInterface->mNCPInstance->change_ncp_state(UNINITIALIZED);
			mInterface->mResetIsExpected = true;

			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				!ncp_state_is_initializing(mInterface->mNCPInstance->get_ncp_state()),
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
				(const EmberKeyData *)mNetworkKey.data(),
				mNetworkKeyIndex
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
			syslog(LOG_INFO, "Setting network key using \"PANAComplete\"");

			emberHostJoinClientComplete(mNetworkKeyIndex, mNetworkKey.data(), mPrefix);

			// Send the command to set the network key and index on the NCP.
			CONTROL_REQUIRE_FLUSH_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				on_error
			);
		}

		// Wait for the response from the NCP.
		EH_REQUIRE_WITHIN(
			NCP_JOIN_TIMEOUT,
			event == EVENT_EMBER_CB_JOIN_NETWORK_RETURN,
			on_error
		);

		ret = EmberGlue::status_to_wpantund_status(va_arg(args, int));
	} else {
		syslog(LOG_ERR, "Not allowed to set NetworkKey in this state.");
		ret = kWPANTUNDStatus_InvalidForCurrentState;
	}

	finish(ret);

	EH_EXIT();

on_error:

	syslog(LOG_ERR, "Set network key failed: %d", ret);

	finish(ret);

	if (ret == kWPANTUNDStatus_Timeout) {
		emberResetMicro();

		mInterface->mNCPInstance->change_ncp_state(UNINITIALIZED);

		EH_YIELD();
	}

	EH_END();
}
