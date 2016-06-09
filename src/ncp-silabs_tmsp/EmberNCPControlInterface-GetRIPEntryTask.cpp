

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
#include "EmberNCPControlInterface-GetRIPEntryTask.h"
#include "EmberNCPUtils.h"
#include "string-utils.h"
#include "Data.h"

using namespace nl;
using namespace nl::wpantund;

#define GET_RIP_ENTRIES_MAX_TIME_TO_COMPLETE	10.0

#ifndef POLL_ALL_RIP_ENTRIES
#define POLL_ALL_RIP_ENTRIES 1
#endif

std::string
nl::wpantund::EmberNCPControlInterface::GetRIPEntryTask::RipEntryToString(uint8_t index, const EmberRipEntry *ripEntry)
{

	if (ripEntry == NULL)
	{
		return std::string("");
	}

	// push only valid entries
	if ( (ripEntry->ripMetric != 0) || (ripEntry->nextHopIndex != 0) ||
		 (ripEntry->incomingLinkQuality != 0) || (ripEntry->incomingLinkQuality != 0) )
	{
		char ripEntryCStr[100];
		char euiStr[21];
		uint8_t longId[8];

		// Ember NCPs always store the EUI64 in reverse order.
		memcpyrev(longId, ripEntry->longId, sizeof(ripEntry->longId));

		// Encode the EUI64 into string format
		encode_data_into_string(longId, sizeof(longId), euiStr, sizeof(euiStr), 8);

		snprintf(ripEntryCStr, sizeof(ripEntryCStr), "[%02d] EUI:%s (%s) RSSI:%d LQI(in/out):%u/%u Sync:%s Age:%d Metric:%d",
			index, euiStr,
			node_type_to_string(EmberGlue::node_type_convert_from_ember(ripEntry->type)).c_str(),
			ripEntry->rollingRssi, ripEntry->incomingLinkQuality, ripEntry->outgoingLinkQuality,
			ripEntry->mleSync ? "y" : "n", ripEntry->age, ripEntry->ripMetric);

		syslog(LOG_INFO, "RIP:%s", ripEntryCStr);
		return std::string(ripEntryCStr);
	}

	return std::string("");
}

ValueMap
nl::wpantund::EmberNCPControlInterface::GetRIPEntryTask::RipEntryToValueMap(uint8_t index, const EmberRipEntry *ripEntry)
{
	(void) index;
	ValueMap ripEntryValueMap;

	if (ripEntry == NULL)
	{
		return ripEntryValueMap;
	}

	// push only valid entries
	if ( (ripEntry->ripMetric != 0) || (ripEntry->nextHopIndex != 0) ||
		 (ripEntry->incomingLinkQuality != 0) || (ripEntry->incomingLinkQuality != 0) )
	{
		uint8_t longId[8];

		// Ember NCPs always store the EUI64 in reverse order.
		memcpyrev(longId, ripEntry->longId, sizeof(ripEntry->longId));

		// Fill the dictionary
		ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_EUI64] = Data(ripEntry->longId, sizeof(ripEntry->longId));
		ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_RSSI] = ripEntry->rollingRssi;
		ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_IncomingLinkQuality] = ripEntry->incomingLinkQuality;
		ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_OutgoingLinkQuality] = ripEntry->outgoingLinkQuality;
		ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_Age] = ripEntry->age;
		ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_NodeType] =
			node_type_to_string(EmberGlue::node_type_convert_from_ember(ripEntry->type));
		ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_MLESync] = ripEntry->mleSync;
		ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_RIPMetric] = ripEntry->ripMetric;
	}

	return ripEntryValueMap;
}


nl::wpantund::EmberNCPControlInterface::GetRIPEntryTask::GetRIPEntryTask(
	EmberNCPControlInterface* interface,
	CallbackWithStatusArg1 cb,
	OutputMode output_mode
):	HelperTask(interface,cb), mIndex(0), mOutputMode(output_mode)
{
}

void
nl::wpantund::EmberNCPControlInterface::GetRIPEntryTask::add_to_output(uint8_t index, const EmberRipEntry *ripEntry)
{
	if (mOutputMode == kOutputModeString) {
		std::string ripString;

		ripString = RipEntryToString(index, ripEntry);

		if (ripString.size() != 0) {
			mStringOutputList.push_back(ripString);
		}
	} else {
		ValueMap ripValueMap;

		ripValueMap = RipEntryToValueMap(index, ripEntry);

		if (!ripValueMap.empty()) {
			mValueMapOutputList.push_back(ripValueMap);
		}
	}
}

int
nl::wpantund::EmberNCPControlInterface::GetRIPEntryTask::vprocess_event(int event, va_list args)
{
	int ret = 0;
	std::string ripString;

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

	if (mInterface->mNCPInstance->mNodeType == LURKER) {
		// Lurkers can't talk to end-devices, so skip them.
		mIndex = 32;
	} else {
		mIndex = 0;
	}

#if POLL_ALL_RIP_ENTRIES

	// Original, slow method for fetching all RIP entries.

	if (ncp_state_has_joined(mInterface->mNCPInstance->get_ncp_state())) {
		for (;mIndex < EMBER_RIP_TABLE_MAX_SIZE; mIndex++) {
			emberGetRipEntry(mIndex);

			CONTROL_REQUIRE_FLUSH_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				on_error
			);

			EH_REQUIRE_WITHIN(
				GET_RIP_ENTRIES_MAX_TIME_TO_COMPLETE/(float)EMBER_RIP_TABLE_MAX_SIZE,
				event == EVENT_EMBER_CB_GET_RIP_ENTRY_RETURN,
				on_error
			);

			// Make sure this is the RIP entry we requested.
			require(va_arg(args, int) == mIndex, on_error);

			add_to_output(mIndex, va_arg(args, const EmberRipEntry *));
		}
	}

#else // #if POLL_ALL_RIP_ENTRIES

	// Faster (but experimental) way to get RIP Entries.

	emberGetRipEntry(0xFF);

	// We use the emberState callback as a way to identify
	// that we have received all of the RIP entries.
	emberState();

	do {
		event = EVENT_NULL;

		EH_REQUIRE_WITHIN(
			GET_RIP_ENTRIES_MAX_TIME_TO_COMPLETE/(float)EMBER_RIP_TABLE_MAX_SIZE,
			event == EVENT_EMBER_CB_GET_RIP_ENTRY_RETURN
			|| event == EVENT_EMBER_CB_STATE_RETURN,
			on_error
		);

		if (event == EVENT_EMBER_CB_GET_RIP_ENTRY_RETURN) {
			mIndex = va_arg(args, int);

			add_to_output(mIndex, va_arg(args, const EmberRipEntry *));
		}
	} while(event == EVENT_EMBER_CB_GET_RIP_ENTRY_RETURN);

#endif // #if !POLL_ALL_RIP_ENTRIES

	if( mOutputMode == kOutputModeString) {
		std::string outputStr;
		std::list<std::string>::iterator it;
		for (it=mStringOutputList.begin(); it != mStringOutputList.end(); )
		{
			outputStr.append(*it);
			++it;
			if ( it != mStringOutputList.end() )
				outputStr.append("\n");
		}
		finish(0, outputStr);
	} else {
		finish(0, boost::any(mValueMapOutputList));
	}

	EH_EXIT();

on_error:

	if (ret == kWPANTUNDStatus_Ok) {
		ret = kWPANTUNDStatus_Failure;
	}

	syslog(LOG_ERR, "GetRipEntriy failed: %d", ret);

	finish(ret);

	EH_END();
}
