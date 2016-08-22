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
		require_action(mInterface->mFirmwareInterface->IsSupported_CMDCommissionNetwork(), on_error, ret = kWPANTUNDStatus_FeatureNotSupported);
		require_action(mInterface->mNetworkKey.size() == 16, on_error, ret = kWPANTUNDStatus_Failure);

		CONTROL_REQUIRE_SENT_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			mInterface->mFirmwareInterface->CMD_CommissionNetwork(
				mNetwork,
				mNetwork.channel,
				EmberCommands::FirmwareInterface::kAllChannelsMask,
				mPrefix,
				mInterface->mNetworkKey.data()
			),
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
			mInterface->mFirmwareInterface->CMD_JoinCommissioned(
				mInterface->mTXPower,
				mInterface->mFirmwareInterface->NodeTypeConvertToEmber(mNodeType, mInterface->mNCPInstance->mLegacyInterfaceEnabled)
			),
			on_error
		);
	} else {
		CONTROL_REQUIRE_SENT_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			mInterface->mFirmwareInterface->CMD_Join(
				mNetwork,
				mInterface->mFirmwareInterface->NodeTypeConvertToEmber(mNodeType, mInterface->mNCPInstance->mLegacyInterfaceEnabled),
				mInterface->mTXPower
			),
			on_error
		);
	}

	// We don't wait for this one because we
	// don't want to miss `mInterface->mFirmwareInterface->GetIDFor_CBForm()`.
	// We send this so that we can get some insight
	// about the current state of the NCP during the
	// joining process.
	mInterface->send_management_command(mInterface->mFirmwareInterface->CMD_GetState());

	EH_REQUIRE_WITHIN(
		NCP_JOIN_TIMEOUT,
		event == mInterface->mFirmwareInterface->GetIDFor_CBJoin()
		|| event == mInterface->mFirmwareInterface->GetIDFor_CBStartHostJoinClient(),
		on_error
	);

	if (event == mInterface->mFirmwareInterface->GetIDFor_CBJoin()) {
		ret = GetNCPCallbackReturnStatus(event, args);

		if (ret) {
			// Fetch the state so that we are sure we are in the correct state.
			// Don't block, so that `ret` stays valid.
			mInterface->send_management_command(mInterface->mFirmwareInterface->CMD_GetState());
		}
	} else if (event == mInterface->mFirmwareInterface->GetIDFor_CBStartHostJoinClient()) {
		int block_size(0);
		const uint8_t* block_data(0);
		nl::Data data(va_arg_as_Data(args));

		mInterface->mNCPInstance->mNodeType = mNodeType;
		mInterface->mNCPInstance->change_ncp_state(CREDENTIALS_NEEDED);

		mInterface->mNCPInstance->mCurrentNetworkInstance = mNetwork;

		// Unpack the "parent address" and pass that to the callback.
		ret = EmberCommands::UnpackArguments(
			data,
			EMBER_PACKED_UINT16 EMBER_PACKED_BLOCK,
			NULL,
			&block_size,
			&block_data
		);

		check_string(ret>=0, strerror(errno));

		// We skip passing the "parent address" if it
		// isn't present or didn't parse properly.
		// (Different firmware versions do different things)
		if (ret < 0) {
			block_size = 0;
		}

		ret = kWPANTUNDStatus_InProgress;
		finish(ret, Data(block_data, block_size));

	}

	if (ret == kWPANTUNDStatus_Ok) {
		mInterface->mNCPInstance->mCurrentNetworkInstance = mNetwork;

		if (!ncp_state_has_joined(mInterface->mNCPInstance->get_ncp_state())) {
			// If we haven't already gotten a state change event
			// indicating that we are joined, ask for one.
			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				mInterface->mFirmwareInterface->CMD_GetState(),
				on_error
			);
			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				event == mInterface->mFirmwareInterface->GetIDFor_CBState(),
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
		IGNORE_RETURN_VALUE(mInterface->send_management_command(
			mInterface->mFirmwareInterface->CMD_Reset()
		));

		mInterface->mNCPInstance->change_ncp_state(UNINITIALIZED);

		EH_YIELD();
	}

	EH_END();
}
