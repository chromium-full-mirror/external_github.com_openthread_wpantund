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
 *      This file contains various definitions and utilities that don't
 *      seem to fit nicely anywhere else.
 *
 */

#ifndef wpantund_EmberNCPUtils_h
#define wpantund_EmberNCPUtils_h

#include "EmberNCPControlInterface.h"
#include "EventHandler.h"

// This callback is not sent from the NCP. It is a fake NCP
// callback sent from the processing thread to indicate that
// the NCP is in deep sleep.
#define EVENT_NCP_DISABLED                 0x78C9

// This callback is not sent from the NCP. It is a fake NCP
// callback sent from the processing thread to indicate that
// the NCP is now fully awake and ready to process commands.
#define EVENT_NCP_NOW_AWAKE                0x78CA

#define EVENT_NCP_CONN_RESET               0x78CB

#define IS_EVENT_FROM_NCP(x)               ((((x) >> 8) & 0xFF) == 'c')



template<class C>
inline nl::wpantund::EmberNCPControlInterface* GetInterface(C *x)
{
	return x->mInterface;
}

template<>
inline nl::wpantund::EmberNCPControlInterface* GetInterface<nl::wpantund::EmberNCPControlInterface>(nl::wpantund::EmberNCPControlInterface *x)
{
	return x;
}

#define CONTROL_REQUIRE_SENT_WITHIN(seconds, packet, error_label) do { \
		static const int ___crsw_send_finished = 0xFF000000 | __LINE__; \
		static const int ___crsw_send_failed = 0xFE000000 | __LINE__; \
		try { \
			int ___crsw_ret = GetInterface(this)->mNCPInstance->send_management_command(packet, CALLBACK_FUNC_SPLIT( \
				boost::bind(&EmberNCPControlInterface::process_event_helper, GetInterface(this), ___crsw_send_finished), \
				boost::bind(&EmberNCPControlInterface::process_event_helper, GetInterface(this), ___crsw_send_failed) \
			)); \
			if (___crsw_ret) { \
				syslog(LOG_ERR, "send_management_command() failed: %d", ___crsw_ret); \
				event = ___crsw_send_failed; \
			} \
		} catch (const std::exception& x) { \
			syslog(LOG_ERR, "Caught exception: %s", x.what()); \
			goto error_label; \
		} \
		EH_WAIT_UNTIL_WITH_TIMEOUT(seconds, (event == ___crsw_send_finished) || (event == ___crsw_send_failed)); \
		require_string(!eh_did_timeout, error_label, "Timed out while trying to send command"); \
		require_string(event == ___crsw_send_finished, error_label, "Failure while trying to send command"); \
	} while (0)

#define CONTROL_REQUIRE_FLUSH_WITHIN(seconds, error_label)	CONTROL_REQUIRE_SENT_WITHIN(seconds, nl::Data(), error_label)


#define BEGIN_TRY_FOR_CMD(cb)	do{}while(0)
#define END_TRY_FOR_CMD(cb)		do{}while(0)

#define BEGIN_TRY_FOR_CMD2(cb)	do{}while(0)
#define END_TRY_FOR_CMD2(cb)	do{}while(0)

#endif
