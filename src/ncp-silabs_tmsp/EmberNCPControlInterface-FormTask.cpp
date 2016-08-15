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
 *		This file contains the code that handles forming a network.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include "EmberNCPControlInterface-FormTask.h"
#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;

nl::wpantund::EmberNCPControlInterface::FormTask::FormTask(
	EmberNCPControlInterface* interface,
	CallbackWithStatusArg1 cb,
	const std::string& network_name,
	NodeType node_type,
	uint32_t channel_mask,
	const uint8_t *ula_prefix,
	const uint8_t *legacy_ula_prefix
):	HelperTask(interface,cb),
	mParameters(),mNetworkName(network_name),mOptions(EMBER_NETWORK_ID_OPTION),
	mChannelMask(channel_mask)
{
	if (legacy_ula_prefix) {
		memcpy(&mParameters.legacyUla, legacy_ula_prefix, sizeof(mParameters.legacyUla));
		mOptions |= EMBER_LEGACY_ULA_OPTION;
	}

	if (ula_prefix) {
		memcpy(&mParameters.ulaPrefix, ula_prefix, sizeof(mParameters.ulaPrefix));
		mOptions |= EMBER_ULA_PREFIX_OPTION;
	}

	if (node_type != UNKNOWN) {
		mParameters.nodeType = EmberGlue::node_type_convert_to_ember(
			node_type,
			interface->mNCPInstance->mLegacyInterfaceEnabled
			 || ((mOptions & EMBER_LEGACY_ULA_OPTION) == EMBER_LEGACY_ULA_OPTION)
		);
		mOptions |= EMBER_NODE_TYPE_OPTION;
	}

	if (mInterface->mTXPower != USE_DEFAULT_TX_POWER) {
		mParameters.radioTxPower = mInterface->mTXPower;
		mOptions |= EMBER_TX_POWER_OPTION;
	}
}

int
nl::wpantund::EmberNCPControlInterface::FormTask::vprocess_event(int event, va_list args)
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
	} else if (mNetworkName.size() > 16) {
		ret = kWPANTUNDStatus_InvalidArgument;
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

	// Ignore any coverty warning for the following line.
	// This is valid and intentional---mParameters.networkId doesn't need
	// to be NULL terminated.
	strncpy((char*)mParameters.networkId, mNetworkName.c_str(), sizeof(mParameters.networkId));

	emberFormNetwork(&mParameters, mOptions, mChannelMask);

	CONTROL_REQUIRE_FLUSH_WITHIN(
		NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
		on_error
	);

	emberState();

	EH_REQUIRE_WITHIN(
		NCP_FORM_TIMEOUT,
		event == EVENT_EMBER_CB_FORM_NETWORK_RETURN,
		on_error
	);

	ret = EmberGlue::status_to_wpantund_status(va_arg(args, int));

	if (EMBER_TYPE_SUPPORTS_LEGACY(mParameters.nodeType)) {
		mInterface->mNCPInstance->mLegacyInterfaceEnabled = true;
	}

#if EMBER_BUILD_NUMBER >= 426
	do {
		EH_REQUIRE_WITHIN(
			NCP_FORM_TIMEOUT,
			event == EVENT_EMBER_CB_NETWORK_STATUS_HANDLER,
			on_error
		);

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

		event = EVENT_IDLE; // Clear event so we do not process it again in next loop.

	} while (true);

	// Fetch the state to ensure wpantund NCP state gets updated (no need to wait for callback).
	emberState();

#else
	if (ret == 0) {
		mInterface->mNCPInstance->change_ncp_state(ASSOCIATED);
	}
#endif

	finish(ret);

	EH_EXIT();

on_error:

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "Form failed: %d", ret);

	finish(ret);

	if (mInterface->mNCPInstance->get_ncp_state() != OFFLINE) {
		emberResetMicro();

		mInterface->mNCPInstance->change_ncp_state(UNINITIALIZED);

		EH_YIELD();
	}

	EH_END();
}
