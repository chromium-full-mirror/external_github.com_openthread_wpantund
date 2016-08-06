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
 *		This file contains the code that handles leaving a network.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include "EmberNCPControlInterface-LeaveTask.h"
#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;

nl::wpantund::EmberNCPControlInterface::LeaveTask::LeaveTask(
	EmberNCPControlInterface* interface,
	CallbackWithStatusArg1 cb
):	HelperTask(interface,cb)
{
}

int
nl::wpantund::EmberNCPControlInterface::LeaveTask::vprocess_event(int event, va_list args)
{
	int ret = kWPANTUNDStatus_Failure;

	EH_BEGIN();

	if (!mInterface->mNCPInstance->mEnabled) {
		ret = kWPANTUNDStatus_InvalidWhenDisabled;
	} else if (mInterface->mNCPInstance->is_initializing_ncp()) {
		ret = kWPANTUNDStatus_Busy;
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


	emberResetNetworkState();

	CONTROL_REQUIRE_FLUSH_WITHIN(
		NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
		on_error
	);

	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		event == EVENT_EMBER_CB_RESET_NETWORK_STATE_RETURN,
		on_error
	);

	ret = EmberGlue::status_to_wpantund_status(va_arg(args, int));

	check_noerr(ret);

	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		mInterface->mNCPInstance->get_ncp_state() == OFFLINE,
		on_error
	);

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

	require(!ncp_state_is_commissioned(mInterface->mNCPInstance->get_ncp_state()), on_error);

	require(!ncp_state_is_joining(mInterface->mNCPInstance->get_ncp_state()), on_error);

	ret = kWPANTUNDStatus_Ok;

	finish(ret);

	EH_EXIT();

on_error:

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "Leave failed: %d", ret);

	finish(ret);

	if (mInterface->mNCPInstance->get_ncp_state() > OFFLINE) {
		emberResetMicro();

		mInterface->mNCPInstance->change_ncp_state(UNINITIALIZED);

		EH_YIELD();
	}

	EH_END();
}
