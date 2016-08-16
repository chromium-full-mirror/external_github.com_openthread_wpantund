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
 *		Task to get global addresses from NCP.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include "EmberNCPControlInterface-GetGlobalAddressesTask.h"
#include "EmberNCPUtils.h"

using namespace nl;
using namespace nl::wpantund;


nl::wpantund::EmberNCPControlInterface::GetGlobalAddressesTask::GetGlobalAddressesTask(
	EmberNCPControlInterface* interface,
	CallbackWithStatusArg1 cb
):	HelperTask(interface,cb)
{
}

int
nl::wpantund::EmberNCPControlInterface::GetGlobalAddressesTask::vprocess_event(int event, va_list args)
{
	int ret = 0;
	struct in6_addr address;
	uint32_t preferred_lifetime;
	uint32_t valid_lifetime;
	uint8_t flags;
	char address_string[INET6_ADDRSTRLEN];
	char c_string[200];

	EH_BEGIN();

	if (!mInterface->mNCPInstance->mEnabled) {
		ret = kWPANTUNDStatus_InvalidWhenDisabled;
		goto on_error;
	}

	// The first event to a task is EVENT_STARTING_TASK. The following
	// line makes sure that we don't start processing this task
	// until it is properly scheduled. All tasks immediately receive
	// the initial `EVENT_STARTING_TASK` event, but further events
	// will only be received by that task once it is that task's turn
	// to execute.
	EH_WAIT_UNTIL(EVENT_STARTING_TASK != event);

	if (ncp_state_has_joined(mInterface->mNCPInstance->get_ncp_state())) {

		emberGetGlobalAddresses(NULL, 0);

		CONTROL_REQUIRE_FLUSH_WITHIN(
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			on_error
		);

		do {

			EH_WAIT_UNTIL_WITH_TIMEOUT(
				1,
				event == EVENT_EMBER_CB_GET_GLOBAL_ADDRESS_RETURN
			);

			if (eh_did_timeout) {
				break;
			}

			address = *va_arg(args, const struct in6_addr *);
			preferred_lifetime = va_arg(args, uint32_t);
			valid_lifetime = va_arg(args, uint32_t);
			flags = va_arg(args, int);

			inet_ntop(AF_INET6,	&address, address_string, sizeof(address_string));
			snprintf(c_string, sizeof(c_string), "%-41s valid: %-10u  preferred: %-10u  flags: %s (0x%02X)",
					address_string,
					valid_lifetime,
					preferred_lifetime,
					address_flags_to_string(flags).c_str(),
					flags
				);

			mOutValue.push_back(std::string(c_string));

			event = EVENT_IDLE;   // Clear event so we do not process it again in next loop

		} while (1);

	}

	finish(0, boost::any(mOutValue));

	EH_EXIT();


on_error:

	if (ret == 0) {
		ret = -1;
	}

	syslog(LOG_ERR, "GetGlobalAddress failed: %d", ret);

	finish(ret);

	EH_END();
}
