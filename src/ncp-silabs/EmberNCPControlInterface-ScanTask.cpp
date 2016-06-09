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
 *		This file contains the code that handles scanning for networks
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include <list>
#include <boost/any.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include "NetworkInstance.h"
#include <boost/enable_shared_from_this.hpp>
#include "EmberNCPControlInterface-ScanTask.h"
#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;


nl::wpantund::EmberNCPControlInterface::ScanTask::ScanTask(EmberNCPControlInterface* interface, CallbackWithStatusArg1 cb, int period, uint32_t channel_mask):
		HelperTask(interface,cb), mPeriod(period), mChannelMask(channel_mask)
{ }

void
nl::wpantund::EmberNCPControlInterface::ScanTask::finish(int status, const boost::any& value)
{
	if (!mCB.empty() && (status != kWPANTUNDStatus_Already)) {
		if (status == kWPANTUNDStatus_Canceled) {
			IGNORE_RETURN_VALUE(mInterface->send_management_command(mInterface->mFirmwareInterface->CMD_StopScan()));
		}
	}
	HelperTask::finish(status, value);
}

int
nl::wpantund::EmberNCPControlInterface::ScanTask::vprocess_event(int event, va_list args)
{
	int ret = 0;

	EH_BEGIN();


	if (!mInterface->mNCPInstance->mEnabled) {
		ret = kWPANTUNDStatus_InvalidWhenDisabled;
		finish(ret);
		EH_EXIT();
	}

	if (mInterface->mNCPInstance->get_ncp_state() == UPGRADING) {
		ret = kWPANTUNDStatus_InvalidForCurrentState;
		finish(ret);
		EH_EXIT();
	}

	// Wait for a bit to see if the NCP will enter the right state.
	EH_REQUIRE_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		!ncp_state_is_initializing(mInterface->mNCPInstance->get_ncp_state())
		&& (mInterface->mNCPInstance->get_ncp_state() != ASSOCIATING)
		&& (mInterface->mNCPInstance->get_ncp_state() != CREDENTIALS_NEEDED)
		&& (mInterface->mNCPInstance->get_ncp_state() != COMMISSIONED),
		on_error
	);

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);


	if (mPeriod == 0) {
		mPeriod = 3;
	}

	CONTROL_REQUIRE_SENT_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		mInterface->mFirmwareInterface->CMD_Scan(mPeriod, mChannelMask),
		on_error
	);

	do {
		EH_REQUIRE_WITHIN(
			30,
			event == mInterface->mFirmwareInterface->GetIDFor_CBActiveScan()
			|| event == mInterface->mFirmwareInterface->GetIDFor_CBScanReturn(),
			on_error
		);
		if (event == mInterface->mFirmwareInterface->GetIDFor_CBActiveScan()) {
			WPAN::NetworkInstance network;
			ret = mInterface->mFirmwareInterface->Parse_CBActiveScan(
				va_arg_as_Data(args),
				&network
			);

			if (ret) {
				int errno_value = -ret;
				syslog(LOG_ERR, "FAILED TO UNPACK ARGUMENTS (errno: %d, %s)", errno_value, strerror(errno_value));
				goto on_error;
			}

			mInterface->mOnNetScanBeacon(network);

			// Change the event type to 'IDLE' so that we
			// don't try to process this event once than once.
			event = EVENT_IDLE;

		} else if (event == mInterface->mFirmwareInterface->GetIDFor_CBScanReturn()) {
			finish(0);
			break;
		}
	} while(true);

	EH_EXIT();

on_error:
	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "Scan failed: %d", ret);

	finish(ret);

	EH_END();
}
