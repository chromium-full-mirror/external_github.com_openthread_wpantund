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

	CONTROL_REQUIRE_SENT_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		mSendCommand,
		on_error
	);

	if (mCallbackEvent == EVENT_NULL) {
		// No callback to check for.
		finish(ret);
		EH_EXIT();
	}

	if (mCallbackEvent == mInterface->mFirmwareInterface->GetIDFor_CBReset()) {
		mInterface->mResetIsExpected = true;
	}

	EH_REQUIRE_WITHIN(
		mTimeout,
		event == mCallbackEvent,
		on_error
	);

	// We can only extract packed values if the callback event from from the NCP,
	// so finish early if this isn't from the NCP.
	if (!IS_EVENT_FROM_NCP(mCallbackEvent)) {
		finish(ret);
		EH_EXIT();
	}

	if (mRetPackedType) {
		nl::Data arg_data(va_arg_as_Data(args));
		if (mRetPackedType == EmberCommands::PacketFieldType::EMBER_PACKED_UINT8_CHAR_T) {
			uint8_t value = 0;
			int bytes_processed = 0;
			bytes_processed = EmberCommands::UnpackArguments(
				arg_data,
				EMBER_PACKED_UINT16 EMBER_PACKED_UINT8,
				NULL, // Skip the UINT16, which has the command id
				&value);
			ret = bytes_processed>0?0:-errno;
			finish(ret, boost::any(value));
			EH_EXIT();
		} else if (mRetPackedType == EmberCommands::PacketFieldType::EMBER_PACKED_INT8_CHAR_T) {
			int8_t value = 0;
			int bytes_processed = 0;
			bytes_processed = EmberCommands::UnpackArguments(
				arg_data,
				EMBER_PACKED_UINT16 EMBER_PACKED_INT8,
				NULL, // Skip the UINT16, which has the command id
				&value);
			ret = bytes_processed>0?0:-errno;
			finish(ret, boost::any(value));
			EH_EXIT();
		} else if (mRetPackedType == EmberCommands::PacketFieldType::EMBER_PACKED_UINT16_CHAR_T) {
			uint16_t value = 0;
			int bytes_processed = 0;
			bytes_processed = EmberCommands::UnpackArguments(
				arg_data,
				EMBER_PACKED_UINT16 EMBER_PACKED_UINT16,
				NULL, // Skip the UINT16, which has the command id
				&value);
			ret = bytes_processed>0?0:-errno;
			finish(ret, boost::any(value));
			EH_EXIT();
		} else if (mRetPackedType == EmberCommands::PacketFieldType::EMBER_PACKED_UINT32_CHAR_T) {
			uint32_t value = 0;
			int bytes_processed = 0;
			bytes_processed = EmberCommands::UnpackArguments(
				arg_data,
				EMBER_PACKED_UINT16 EMBER_PACKED_UINT32,
				NULL, // Skip the UINT16, which has the command id
				&value);
			ret = bytes_processed>0?0:-errno;
			finish(ret, boost::any(value));
			EH_EXIT();
		} else if (mRetPackedType == EmberCommands::PacketFieldType::EMBER_PACKED_BLOCK_CHAR_T) {
			int block_size = 0;
			const uint8_t* block_data = NULL;
			int bytes_processed = 0;
			bytes_processed = EmberCommands::UnpackArguments(
				arg_data,
				EMBER_PACKED_UINT16 EMBER_PACKED_BLOCK,
				&block_size,
				&block_data);
			ret = bytes_processed>0?0:-errno;
			finish(ret, Data(block_data, block_size));
			EH_EXIT();
		} else {
			syslog(LOG_ERR, "Logic error, bad packed return type enumeration for SendCommandTask");
			goto on_error;
		}

	} else {
		ret = GetNCPCallbackReturnStatus(event, args);
	}

	if (mCallbackEvent == mInterface->mFirmwareInterface->GetIDFor_CBReset()) {
		EH_REQUIRE_WITHIN(
			mTimeout,
			!ncp_state_is_initializing(mInterface->mNCPInstance->get_ncp_state()),
			on_error
		);
	}

	finish(ret, EmberCommands::StripNCPHeader(va_arg_as_Data(args)));

	EH_EXIT();

on_error:

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(
		LOG_ERR,
		"Failure for command \"%s\" (expecting %s): %d",
		mInterface->mFirmwareInterface->CommandIdentifierToString(mInterface->mFirmwareInterface->GetCMDID(mSendCommand)).c_str(),
		mInterface->mFirmwareInterface->CommandIdentifierToString(mCallbackEvent).c_str(),
		ret
	);

	finish(ret);

	EH_END();
}
