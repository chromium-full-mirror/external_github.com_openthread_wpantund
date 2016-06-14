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
#include "EmberNCPControlInterface-SendCommandTask.h"
#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;

nl::wpantund::EmberNCPControlInterface::SendCommandTask::SendCommandTask(
	EmberNCPControlInterface* interface,
	CallbackWithStatusArg1 cb,
	const Data& send_command,
	int callback_event,
	int timeout,
	EmberCommands::PacketFieldType ret_packed_type
):	HelperTask(interface,cb), mCallbackEvent(callback_event),
mTimeout(timeout), mSendCommand(send_command), mRetPackedType(ret_packed_type)
{
}

nl::wpantund::EmberNCPControlInterface::SendCommandTask::SendCommandTask(EmberNCPControlInterface* interface,
	const CallbackWithStatusArg1& cb,
	const CallbackSimple& send_command_callback,
	int callback_event,
	int timeout,
	EmberCommands::PacketFieldType ret_packed_type
):	HelperTask(interface,cb), mCallbackEvent(callback_event),
mTimeout(timeout), mSendCommandCallback(send_command_callback), mRetPackedType(ret_packed_type)
{
}

static Data
StripNCPHeader(const Data& data_in)
{
	Data data(data_in);

	if (data.size() >= 2) {
		data.erase(data.begin(), data.begin() + 2);
	}

	return data;
}

int
nl::wpantund::EmberNCPControlInterface::SendCommandTask::vprocess_event(int event, va_list args)
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

	if (mSendCommandCallback.empty()) {
		// The old way
		CONTROL_REQUIRE_SENT_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			mSendCommand,
			on_error
		);
	} else {
		// The new way
		mSendCommandCallback();

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
			on_error
		);
	}

	if (mCallbackEvent == EVENT_NULL) {
		// No callback to check for.
		finish(ret);
		EH_EXIT();
	}

	if (mCallbackEvent == EVENT_EMBER_CB_RESET_MICRO_HANDLER) {
		mInterface->mResetIsExpected = true;
	}

	EH_REQUIRE_WITHIN(
		mTimeout,
		event == mCallbackEvent,
		on_error
	);

	// We can only extract packed values if the callback event from from the NCP,
	// so finish early if this isn't from the NCP.
	if (!IS_EVENT_FROM_NCP(mCallbackEvent) && !IS_EVENT_FROM_EMBER(mCallbackEvent)) {
		finish(ret);
		EH_EXIT();
	}

	if (mRetPackedType && IS_EVENT_FROM_EMBER(mCallbackEvent)) {
		if (mRetPackedType == EmberCommands::PacketFieldType::EMBER_PACKED_UINT8_CHAR_T) {
			finish(ret, boost::any(va_arg_small(args, uint8_t)));
			EH_EXIT();
		} else if (mRetPackedType == EmberCommands::PacketFieldType::EMBER_PACKED_INT8_CHAR_T) {
			finish(ret, boost::any(va_arg_small(args, int8_t)));
			EH_EXIT();
		} else if (mRetPackedType == EmberCommands::PacketFieldType::EMBER_PACKED_UINT16_CHAR_T) {
			finish(ret, boost::any(va_arg_small(args, uint16_t)));
			EH_EXIT();
		} else if (mRetPackedType == EmberCommands::PacketFieldType::EMBER_PACKED_UINT32_CHAR_T) {
			finish(ret, boost::any(va_arg(args, uint32_t)));
			EH_EXIT();
		} else if (mRetPackedType == EmberCommands::PacketFieldType::EMBER_PACKED_BLOCK_CHAR_T) {
			const uint8_t* block_data = va_arg(args, const uint8_t*);
			int block_size = va_arg(args, int);
			finish(ret, Data(block_data, block_size));
			EH_EXIT();
		} else {
			syslog(LOG_ERR, "Logic error, bad packed return type enumeration for SendCommandTask");
			goto on_error;
		}

	} else {
		ret = peek_at_ember_event_status(event, args);
	}

	if (IS_EVENT_FROM_NCP(event)) {
		finish(ret, StripNCPHeader(va_arg_as_Data(args)));
	} else {
		finish(ret);
	}

	if (mCallbackEvent == EVENT_EMBER_CB_RESET_MICRO_HANDLER) {
		EH_REQUIRE_WITHIN(
			mTimeout,
			!ncp_state_is_initializing(mInterface->mNCPInstance->get_ncp_state()),
			on_error
		);
	}

	EH_EXIT();

on_error:

	check_noerr(ret);

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "SendCommandTask Failure: %d", ret);

	finish(ret);

	EH_END();
}
