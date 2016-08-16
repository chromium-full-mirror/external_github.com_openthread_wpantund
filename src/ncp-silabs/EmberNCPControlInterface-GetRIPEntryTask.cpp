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

using namespace nl;
using namespace nl::wpantund;

#define GET_RIP_ENTRIES_MAX_TIME_TO_COMPLETE	10.0

typedef struct {
  uint8_t eui64[8];
  uint8_t type;
  int8_t rollingRssi;
  uint8_t nextHopIndex;
  uint8_t ripMetric;
  uint8_t incomingLinkQuality;
  uint8_t outgoingLinkQuality;
  bool mleSync;
  uint8_t age;
} EmberRipEntry;

#define EMBER_RIP_TABLE_MAX_COUNT 64

std::string
nl::wpantund::EmberNCPControlInterface::GetRIPEntryTask::RipEntryToString(const Data& data_in)
{
	const uint8_t* data(data_in.data());
	size_t len(data_in.size());

	if ((data != NULL) && len) {
		uint8_t index = data[2];
		EmberRipEntry ripEntry;

		memcpy(&ripEntry, &data[4], sizeof(ripEntry));

		// push only valid entries
		if ( (ripEntry.ripMetric != 0) || (ripEntry.nextHopIndex != 0) ||
			 (ripEntry.incomingLinkQuality != 0) || (ripEntry.incomingLinkQuality != 0) )
		{
			char ripEntryCStr[100];
			char euiStr[21];

			// Ember NCPs always store the EUI64 in reverse order.
			reverse_bytes(ripEntry.eui64, sizeof(ripEntry.eui64));

			// Encode the EUI64 into string format
			encode_data_into_string(ripEntry.eui64, sizeof(ripEntry.eui64), euiStr, sizeof(euiStr), 8);

			snprintf(ripEntryCStr, sizeof(ripEntryCStr), "[%02d] EUI:%s (%s) RSSI:%d LQI(in/out):%u/%u Sync:%s Age:%d Metric:%d",
				index, euiStr,
				node_type_to_string(mInterface->mFirmwareInterface->NodeTypeConvertFromEmber(ripEntry.type)).c_str(),
				ripEntry.rollingRssi, ripEntry.incomingLinkQuality, ripEntry.outgoingLinkQuality,
				ripEntry.mleSync ? "y" : "n", ripEntry.age, ripEntry.ripMetric);

			syslog(LOG_INFO, "RIP:%s", ripEntryCStr);
			return std::string(ripEntryCStr);
		}
	}

	return std::string("");
}

ValueMap
nl::wpantund::EmberNCPControlInterface::GetRIPEntryTask::RipEntryToValueMap(const Data& data_in) {
	const uint8_t* data(data_in.data());
	size_t len(data_in.size());
	ValueMap ripEntryValueMap;

	if ((data != NULL) && len) {
		EmberRipEntry ripEntry;

		memcpy(&ripEntry, &data[4], sizeof(ripEntry));

		// push only valid entries
		if ( (ripEntry.ripMetric != 0) || (ripEntry.nextHopIndex != 0) ||
			 (ripEntry.incomingLinkQuality != 0) || (ripEntry.incomingLinkQuality != 0) )
		{
			// Ember NCPs always store the EUI64 in reverse order.
			reverse_bytes(ripEntry.eui64, sizeof(ripEntry.eui64));

			// Fill the ValueMap with the RIP entry data.
			ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_EUI64] = Data(ripEntry.eui64, sizeof(ripEntry.eui64));
			ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_RSSI] = ripEntry.rollingRssi;
			ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_IncomingLinkQuality] = ripEntry.incomingLinkQuality;
			ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_OutgoingLinkQuality] = ripEntry.outgoingLinkQuality;
			ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_Age] = ripEntry.age;
			ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_NodeType] =
				node_type_to_string(mInterface->mFirmwareInterface->NodeTypeConvertFromEmber(ripEntry.type));
			ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_MLESync] = ripEntry.mleSync;
			ripEntryValueMap[kWPANTUNDValueMapKey_Silabs_RIPEntry_RIPMetric] = ripEntry.ripMetric;
		}
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

int
nl::wpantund::EmberNCPControlInterface::GetRIPEntryTask::vprocess_event(int event, va_list args)
{
	int ret = 0;
	std::string ripString;
	ValueMap ripValueMap;

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

	if (ncp_state_has_joined(mInterface->mNCPInstance->get_ncp_state())) {
		for (;mIndex < EMBER_RIP_TABLE_MAX_COUNT; mIndex++) {
			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				mInterface->mFirmwareInterface->CMD_GetRIPEntry(mIndex),
				on_error
			);

			EH_REQUIRE_WITHIN(
				GET_RIP_ENTRIES_MAX_TIME_TO_COMPLETE/(float)EMBER_RIP_TABLE_MAX_COUNT,
				event == mInterface->mFirmwareInterface->GetIDFor_CBRIPEntry(),
				on_error
			);

			if (mOutputMode == kOutputModeString) {
				ripString = RipEntryToString(va_arg_as_Data(args));

				if (ripString.size() != 0) {
					mStringOutputList.push_back(ripString);
				}
			} else {
				ripValueMap = RipEntryToValueMap(va_arg_as_Data(args));
				if (!ripValueMap.empty()) {
					mValueMapOutputList.push_back(ripValueMap);
				}

			}
		}
	}

	if (mOutputMode == kOutputModeString) {
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
		finish(0, mValueMapOutputList);
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
