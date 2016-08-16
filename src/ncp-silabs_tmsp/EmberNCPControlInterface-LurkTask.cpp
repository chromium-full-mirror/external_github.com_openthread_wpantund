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
 *		Writeme!
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include "EmberNCPControlInterface-LurkTask.h"
#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;

nl::wpantund::EmberNCPControlInterface::LurkTask::LurkTask(
	EmberNCPControlInterface* interface,
	CallbackWithStatusArg1 cb
):	HelperTask(interface,cb), mOriginalState(mInterface->mNCPInstance->get_ncp_state())
{
}

void
nl::wpantund::EmberNCPControlInterface::LurkTask::finish(int status, const boost::any& value)
{

	if (!mCB.empty()
		&& (status != 0)
		&& (mInterface->mNCPInstance->get_ncp_state() == NET_WAKE_ASLEEP)
	) {
		mInterface->mNCPInstance->change_ncp_state(mOriginalState);
	}

	HelperTask::finish(status, value);
}

int
nl::wpantund::EmberNCPControlInterface::LurkTask::vprocess_event(int event, va_list args)
{
	int ret = 0;

	EH_BEGIN();

	if (!mInterface->mNCPInstance->mEnabled) {
		ret = kWPANTUNDStatus_InvalidWhenDisabled;
	} else if (NET_WAKE_ASLEEP == mInterface->mNCPInstance->get_ncp_state()) {
		ret = kWPANTUNDStatus_Already;
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

	emberListenForWakeup(EMBER_SLEEP);

	CONTROL_REQUIRE_FLUSH_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		on_error
	);

	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		event == EVENT_EMBER_CB_LISTEN_FOR_WAKEUP_RETURN,
		on_error
	);

	ret = EmberGlue::status_to_wpantund_status(va_arg(args, int));

	require_noerr(ret, on_error);

	mInterface->mNCPInstance->change_ncp_state(NET_WAKE_ASLEEP);

	finish(0);

	EH_EXIT();

on_error:
	syslog(LOG_ERR, "Unable to get NCP to lurk!");
	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}
	finish(ret);

	EH_END();
}
