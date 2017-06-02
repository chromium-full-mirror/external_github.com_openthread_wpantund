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
 *		This file contains the code that handles transitioning the
 *      NCP into a deep-sleep state.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include "EmberNCPControlInterface-DeepSleepTask.h"
#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;

nl::wpantund::EmberNCPControlInterface::DeepSleepTask::DeepSleepTask(
	EmberNCPControlInterface* interface,
	CallbackWithStatusArg1 cb
):	HelperTask(interface,cb)
{
}

void
nl::wpantund::EmberNCPControlInterface::DeepSleepTask::finish(int status, const boost::any& value)
{
	mInterface->mResetIsExpected = false;
	mInterface->mIsGoingIntoDeepSleep = false;

	HelperTask::finish(status, value);
}


int
nl::wpantund::EmberNCPControlInterface::DeepSleepTask::vprocess_event(int event, va_list args)
{
	int ret = kWPANTUNDStatus_Failure;

	EH_BEGIN();

	// This will be set back to false when this task
	// is finished.
	mInterface->mIsGoingIntoDeepSleep = true;

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);

	// If we are still initializing, wait until we are finished.
	EH_WAIT_UNTIL(mInterface->mDriverState == NORMAL_OPERATION);

	if (mInterface->mNCPInstance->can_set_ncp_power()) {
		// Send a command to the NCP and wait for the response to
		// "flush" out any commands.
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

		// Wait for half a second after the last ncp-generated event before
		// manually cutting the power, just to be conservative.
		do {
			EH_WAIT_UNTIL(!IS_EVENT_FROM_NCP(event));
			EH_WAIT_UNTIL_WITH_TIMEOUT(0.5, IS_EVENT_FROM_NCP(event));
		} while(!eh_did_timeout);

		if (mInterface->mNCPInstance->set_ncp_power(false) == kWPANTUNDStatus_Ok) {
			mInterface->mNCPInstance->change_ncp_state(DEEP_SLEEP);
		} else {
			syslog(LOG_ERR, "DeepSleep: set_ncp_power(false) failed.");

			// Turning off the power manually didn't work for some reason.
			// Turn it back on and we will try to do it via the API.
			mInterface->mNCPInstance->set_ncp_power(true);
		}
	}

	if (mInterface->mNCPInstance->get_ncp_state() != DEEP_SLEEP) {
		syslog(LOG_NOTICE, "DeepSleep: Putting NCP to sleep.");

		CONTROL_REQUIRE_SENT_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			mInterface->mFirmwareInterface->CMD_SleepNoTimer(),
			on_error
		);

		mInterface->mIsGoingIntoDeepSleep = true;
		mInterface->mResetIsExpected = false;
		mInterface->mNetworkKey = nl::Data();
		mInterface->mNetworkKeyIndex = 0;

		syslog(LOG_INFO, "Waiting for NCP to go to sleep...");

		EH_REQUIRE_WITHIN(
			3,
			event == mInterface->mFirmwareInterface->GetIDFor_CBSleepNoTimer(),
			on_error
		);

		ret = GetNCPCallbackReturnStatus(event, args);

		require_noerr(ret, on_error);

		if (mInterface->mFirmwareInterface->Workaround_WaitForCBListenAfterSleepNoTimer()) {
			EH_WAIT_UNTIL(!IS_EVENT_FROM_NCP(event));

			// Wait a little bit to see if we get a spurious mInterface->mFirmwareInterface->GetIDFor_CBListen().
			// #237 work-around
			EH_WAIT_UNTIL_WITH_TIMEOUT(1, IS_EVENT_FROM_NCP(event));

			require(eh_did_timeout || (event == mInterface->mFirmwareInterface->GetIDFor_CBListen()), on_error);
		}

		mInterface->mNCPInstance->change_ncp_state(DEEP_SLEEP);
	}

on_error:

	if (mInterface->mNCPInstance->get_ncp_state() == DEEP_SLEEP) {
		syslog(LOG_NOTICE, "NCP is asleep.");
		ret = kWPANTUNDStatus_Ok;
	} else {
		syslog(LOG_WARNING, "NCP DID NOT GO TO SLEEP!");
	}

	finish(ret);

	EH_END();
}
