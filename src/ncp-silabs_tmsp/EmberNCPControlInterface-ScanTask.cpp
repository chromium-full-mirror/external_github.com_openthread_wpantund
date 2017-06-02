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


nl::wpantund::EmberNCPControlInterface::ScanTask::ScanTask(
	EmberNCPControlInterface* interface,
	CallbackWithStatusArg1 cb,
	ScanType scan_type,
	int period,
	uint32_t channel_mask
):
	HelperTask(interface,cb),
	mScanType(scan_type),
	mPeriod(period),
	mChannelMask(channel_mask)
{ }

void
nl::wpantund::EmberNCPControlInterface::ScanTask::finish(int status, const boost::any& value)
{
	if (!mCB.empty() && (status != kWPANTUNDStatus_Already)) {
		if (status == kWPANTUNDStatus_Canceled) {
			emberStopScan();
		}
	}
	HelperTask::finish(status, value);
}

int
nl::wpantund::EmberNCPControlInterface::ScanTask::vprocess_event(int event, va_list args)
{
	int ret = 0;
	const EmberMacBeaconData *beaconData;

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
		&& !mInterface->mNCPInstance->is_initializing_ncp()
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

	emberStartScan(
		(mScanType == kScanTypeNetScan) ? EMBER_ACTIVE_SCAN : EMBER_ENERGY_SCAN,
		mChannelMask,
		mPeriod
	);

	CONTROL_REQUIRE_FLUSH_WITHIN(
		NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		on_error
	);

	do {
		EH_REQUIRE_WITHIN(
			30,
			event == EVENT_EMBER_CB_ACTIVE_SCAN_HANDLER
			|| event == EVENT_EMBER_CB_SCAN_RETURN
			|| event == EVENT_EMBER_CB_ENERGY_SCAN_HANDLER,
			on_error
		);

		if (event == EVENT_EMBER_CB_ACTIVE_SCAN_HANDLER) {
			char network_name[17] = {};

			beaconData = va_arg(args, const EmberMacBeaconData *);

			memcpy(network_name, beaconData->networkId, sizeof(beaconData->networkId));

			WPAN::NetworkInstance network(
				network_name,
				beaconData->extendedPanId,
				beaconData->panId,
				beaconData->channel,
				beaconData->allowingJoin
			);
			network.rssi = beaconData->rssi;
			network.type = beaconData->protocolId;
			network.version = beaconData->version;

			memcpyrev(network.hwaddr, beaconData->longId, 8);

			if (ret) {
				int errno_value = -ret;
				syslog(LOG_ERR, "FAILED TO UNPACK ARGUMENTS (errno: %d, %s)", errno_value, strerror(errno_value));
				goto on_error;
			}

			mInterface->mOnNetScanBeacon(network);

			// Change the event type to 'IDLE' so that we
			// don't try to process this event once than once.
			event = EVENT_IDLE;

		} else if (event == EVENT_EMBER_CB_ENERGY_SCAN_HANDLER) {

			EnergyScanResultEntry scan_result;

			scan_result.mChannel = va_arg_small(args, uint8_t);
			scan_result.mMaxRssi = va_arg_small(args, int8_t);

			mInterface->mOnEnergyScanResult(scan_result);

			// Change the event type to 'IDLE' so that we
			// don't try to process this event once than once.
			event = EVENT_IDLE;

		} else if (event == EVENT_EMBER_CB_SCAN_RETURN) {
			ret = EmberGlue::status_to_wpantund_status(va_arg(args, int));
			finish(ret);
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
