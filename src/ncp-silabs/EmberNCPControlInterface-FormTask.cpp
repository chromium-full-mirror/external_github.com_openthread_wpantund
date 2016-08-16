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
	mFormCommand(interface->mFirmwareInterface->CMD_Form(
		WPAN::NetworkInstance(network_name),
		interface->mFirmwareInterface->NodeTypeConvertToEmber(node_type, interface->mNCPInstance->mLegacyInterfaceEnabled),
		channel_mask,
		legacy_ula_prefix,
		interface->mTXPower,
		ula_prefix
	))
{
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
	} else if (ncp_state_is_joining_or_joined(mInterface->mNCPInstance->get_ncp_state())) {
		ret = kWPANTUNDStatus_Already;
	} else if (ncp_state_is_commissioned(mInterface->mNCPInstance->get_ncp_state())) {
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

	CONTROL_REQUIRE_SENT_WITHIN(
		NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
		mFormCommand,
		on_error
	);

	// We don't wait for this one because we
	// don't want to miss `mInterface->mFirmwareInterface->GetIDFor_CBForm()`.
	// We send this so that we can get some insight
	// about the current state of the NCP during the
	// joining process.
	mInterface->send_management_command(mInterface->mFirmwareInterface->CMD_GetState());

	EH_REQUIRE_WITHIN(
		NCP_FORM_TIMEOUT,
		event == mInterface->mFirmwareInterface->GetIDFor_CBForm(),
		on_error
	);

	ret = GetNCPCallbackReturnStatus(event, args);

	if (ret == 0) {
		mInterface->mNCPInstance->change_ncp_state(ASSOCIATED);
	}

	finish(ret);

	EH_EXIT();

on_error:

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "Form failed: %d", ret);

	finish(ret);

	if (mInterface->mNCPInstance->get_ncp_state() != OFFLINE) {
		IGNORE_RETURN_VALUE(mInterface->send_management_command(
			mInterface->mFirmwareInterface->CMD_Reset()
		));

		mInterface->mNCPInstance->change_ncp_state(UNINITIALIZED);

		EH_YIELD();
	}

	EH_END();
}
