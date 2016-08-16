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
#include "EmberNCPControlInterface-MfgFinish.h"
#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;

nl::wpantund::EmberNCPControlInterface::MfgFinish::MfgFinish(
	EmberNCPControlInterface* interface,
	CallbackWithStatusArg1 cb
):	HelperTask(interface, cb)
{
}

int
nl::wpantund::EmberNCPControlInterface::MfgFinish::vprocess_event(int event, va_list args)
{
	int ret = 0;
	uint32_t rx_packets = 0;

	EH_BEGIN();

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);

	// Send the command.
	CONTROL_REQUIRE_SENT_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		mInterface->mFirmwareInterface->CMD_MfgEndTest(),
		on_error
	);

	// Wait for the response.
	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		event == mInterface->mFirmwareInterface->GetIDFor_CBMfgEndTest(),
		on_error
	);

	ret = GetNCPCallbackReturnStatus(event, args);

	mInterface->mMfgTestMode = false;

	if (ret == 0) {
		ret = mInterface->mFirmwareInterface->Parse_CBEndTest(va_arg_as_Data(args), &rx_packets);
		syslog(
			LOG_NOTICE,
			"MFG-DIAGS: Finished. Received %d packets total.", rx_packets
		);
	}

	finish(ret, rx_packets);

	EH_EXIT();

on_error:
	syslog(
		LOG_ERR,
		"MfgFinish Failed (ret = %d)",
		ret
	);
	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}
	finish(ret);

	EH_END();
}
