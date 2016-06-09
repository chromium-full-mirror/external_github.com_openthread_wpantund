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
#include "EmberNCPControlInterface-GetCounter.h"
#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;


nl::wpantund::EmberNCPControlInterface::GetCounter::GetCounter(
	EmberNCPControlInterface* interface,
	CallbackWithStatusArg1 cb,
	int counter_index
): HelperTask(interface,cb), mCounterIndex(counter_index)
{
}

int
nl::wpantund::EmberNCPControlInterface::GetCounter::vprocess_event(int event, va_list args)
{
	int ret = 0;

	EH_BEGIN();

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);

	emberGetCounter(mCounterIndex);

	CONTROL_REQUIRE_FLUSH_WITHIN(
		NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
		on_error
	);

	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		(event == EVENT_EMBER_CB_GET_COUNTER_RETURN),
		on_error
	);

	require(va_arg_small(args, EmberCounterType) == mCounterIndex, on_error);

	finish(ret, va_arg_small(args, uint16_t));

	EH_EXIT();

on_error:

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "GetCounter Failure: %d", ret);

	finish(ret);

	EH_END();
}
