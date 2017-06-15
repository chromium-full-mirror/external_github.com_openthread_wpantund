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

nl::Data
EmberNCPControlInterface::SetNetworkKeyTask::CMD_HostJoinClientComplete(void)
{
	uint8_t * ml_prefix = mInterface->mNCPInstance->mNCPV6Prefix;
	const uint8_t * xpanid = mInterface->mNCPInstance->get_current_network_instance().xpanid;

	// Default ML Prefix to be derived from XPANID.
	uint8_t mesh_local_prefix[8] = {
		0xfd, xpanid[0], xpanid[1], xpanid[2], xpanid[3], xpanid[4], 0, 0
	};

	// Actually, user had set an explicit ML Prefix, so copy that in instead.
	if (buffer_is_nonzero(ml_prefix, 8)) {
		memcpy(mesh_local_prefix, ml_prefix, 8);
	}

	return mInterface->mFirmwareInterface->CMD_HostJoinClientComplete(mNetworkKey.data(), mNetworkKeyIndex, mesh_local_prefix);
}

nl::Data
EmberNCPControlInterface::SetNetworkKeyTask::CMD_CommissionNetwork(void)
{
	uint8_t * ml_prefix = mInterface->mNCPInstance->mNCPV6Prefix;
	const uint8_t * xpanid = mInterface->mNCPInstance->get_current_network_instance().xpanid;

	// Default ML Prefix to be derived from XPANID.
	uint8_t mesh_local_prefix[8] = {
		0xfd, xpanid[0], xpanid[1], xpanid[2], xpanid[3], xpanid[4], 0, 0
	};

	// Actually, user had set an explicit ML Prefix, so copy that in instead.
	if (buffer_is_nonzero(ml_prefix, 8)) {
		memcpy(mesh_local_prefix, ml_prefix, 8);
	}

	return mInterface->mFirmwareInterface->CMD_CommissionNetwork(
		mInterface->mNCPInstance->get_current_network_instance(),
		mInterface->mNCPInstance->get_current_network_instance().channel,
		EmberCommands::FirmwareInterface::kAllChannelsMask,
		mesh_local_prefix,
		mNetworkKey.data()
	);
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

			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				mInterface->mFirmwareInterface->CMD_Reset(),
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

		syslog(LOG_NOTICE, "property_set_value: Setting network key using \"SetSecurityParams\"");

		// Send the command to set the network key to the NCP.
		CONTROL_REQUIRE_SENT_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			mInterface->mFirmwareInterface->CMD_SetSecurityParams(mNetworkKey.data()),
			on_error
		);

		// Wait for the response from the NCP.
		EH_REQUIRE_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			event == mInterface->mFirmwareInterface->GetIDFor_CBSetSecurityParams(),
			on_error
		);

		// Interpret the return value from the response.
		ret = GetNCPCallbackReturnStatus(event, args);


	} else if (mInterface->mNCPInstance->get_ncp_state() == CREDENTIALS_NEEDED) {
		if (mInterface->mFirmwareInterface->Workaround_CommissionNetworkRequired()
			&& (mInterface->mNCPInstance->mNodeType != LURKER)
		) {
			mInterface->mResetIsExpected = true;

			mCommissionNetworkData = CMD_CommissionNetwork();
			mJoinCommissionedData = mInterface->mFirmwareInterface->CMD_JoinCommissioned(
				mInterface->mTXPower,
				mInterface->mFirmwareInterface->NodeTypeConvertToEmber(mInterface->mNCPInstance->mNodeType, mInterface->mNCPInstance->mLegacyInterfaceEnabled)
			);

			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				mInterface->mFirmwareInterface->CMD_Reset(),
				on_error
			);

			mInterface->mNCPInstance->change_ncp_state(UNINITIALIZED);
			mInterface->mResetIsExpected = true;

			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				!ncp_state_is_initializing(mInterface->mNCPInstance->get_ncp_state()),
				on_error
			);

			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				mCommissionNetworkData,
				on_error
			);

			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				event == mInterface->mFirmwareInterface->GetIDFor_CBCommissionNetwork(),
				on_error
			);

			ret = GetNCPCallbackReturnStatus(event, args);

			require_noerr(ret, on_error);

			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				mJoinCommissionedData,
				on_error
			);

		} else {
			syslog(LOG_INFO, "Setting network key using \"PANAComplete\"");

			// Send the command to set the network key and index on the NCP.
			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				CMD_HostJoinClientComplete(),
				on_error
			);
		}

		// Wait for the response from the NCP.
		EH_REQUIRE_WITHIN(
			NCP_JOIN_TIMEOUT,
			event == mInterface->mFirmwareInterface->GetIDFor_CBJoin(),
			on_error
		);

		// Interpret the return value from the response.
		ret = GetNCPCallbackReturnStatus(event, args);
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
		IGNORE_RETURN_VALUE(mInterface->send_management_command(
			mInterface->mFirmwareInterface->CMD_Reset()
		));

		mInterface->mNCPInstance->change_ncp_state(UNINITIALIZED);

		EH_YIELD();
	}

	EH_END();
}
