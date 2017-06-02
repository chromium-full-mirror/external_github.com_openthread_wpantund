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
 *		This file contains the code that handles waking up the NCP.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include "EmberNCPControlInterface-WakeupTask.h"
#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;

nl::wpantund::EmberNCPControlInterface::WakeupTask::WakeupTask(
	EmberNCPControlInterface* interface,
	CallbackWithStatusArg1 cb
):	HelperTask(interface,cb)
{
}

void
nl::wpantund::EmberNCPControlInterface::WakeupTask::finish(int status, const boost::any& value)
{
	mInterface->mIsWakingUpFromSleep = false;
	mInterface->mResetIsExpected = false;

	HelperTask::finish(status, value);
}

int
nl::wpantund::EmberNCPControlInterface::WakeupTask::vprocess_event(int event, va_list args)
{
	int ret = 0;

	EH_BEGIN();

	mInterface->mIsWakingUpFromSleep = true;

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);

	mInterface->mNCPInstance->set_ncp_power(true);
	mInterface->mResetIsExpected = true;

	emberFfWakeup();

	CONTROL_REQUIRE_FLUSH_WITHIN(
		NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
		on_error
	);

	if ((mInterface->mNCPInstance->get_ncp_state() == DEEP_SLEEP) || mInterface->mAlwaysResetToWake) {
		mInterface->mResetIsExpected = true;

		emberResetMicro();

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			on_error
		);

		EH_REQUIRE_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			event == EVENT_EMBER_CB_RESET_MICRO_HANDLER,
			on_error
		);
	} else {
	    // Send a GetState() command to ensure we get a callback even if NCP is already awake
		emberState();

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			on_error
		);
	}

	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		!ncp_state_is_sleeping(mInterface->mNCPInstance->get_ncp_state())
		  && (!ncp_state_is_initializing(mInterface->mNCPInstance->get_ncp_state())),
		on_error
	);

	ret = kWPANTUNDStatus_Ok;

	finish(ret);

	EH_EXIT();

on_error:

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "Wake failed: %d", ret);

	mInterface->reinitialize_ncp();

	finish(ret);

	EH_END();
}
