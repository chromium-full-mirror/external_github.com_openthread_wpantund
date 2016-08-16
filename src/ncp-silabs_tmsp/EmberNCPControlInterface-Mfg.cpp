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
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"

#include "CallbackStore.hpp"
#include "config-file.h"
#include "nlpt.h"
#include "string-utils.h"
#include "any-to.h"
#include "time-utils.h"
#include <syslog.h>

#include "EmberNCPControlInterface-SendCommandTask.h"
#include "EmberNCPControlInterface-MfgFinish.h"

#include "stack/include/mfglib.h"

using namespace nl;
using namespace wpantund;


void
EmberNCPControlInterface::mfg_start(CallbackWithStatus cb)
{
	BEGIN_TRY_FOR_CMD(cb);
	start_new_helper_task(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			boost::bind(mfglibStart,mfglibRxHandler),
			EVENT_EMBER_CB_MFGLIB_START_RETURN
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_finish(CallbackWithStatusArg1 cb)
{
	BEGIN_TRY_FOR_CMD2(cb);
	start_new_helper_task(boost::shared_ptr<HelperTask>(
		new MfgFinish(
			this,
			cb
		)
	));
	END_TRY_FOR_CMD2(cb);
}

void
EmberNCPControlInterface::mfg_begin_test(uint8_t type, CallbackWithStatus cb)
{
	BEGIN_TRY_FOR_CMD(cb);
	switch(type) {
	case TONE:
		start_new_helper_task(boost::shared_ptr<HelperTask>(
			new SendCommandTask(
				this,
				boost::bind(cb,_1),
				mfglibStartTone,
				EVENT_EMBER_CB_MFGLIB_START_TONE_RETURN
			)
		));
		break;
	case STREAM:
		start_new_helper_task(boost::shared_ptr<HelperTask>(
			new SendCommandTask(
				this,
				boost::bind(cb,_1),
				mfglibStartStream,
				EVENT_EMBER_CB_MFGLIB_START_STREAM_RETURN
			)
		));
		break;
	default:
		cb(kWPANTUNDStatus_InvalidArgument);
		return;
		break;
	}
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_end_test(uint8_t type, CallbackWithStatus cb)
{
	BEGIN_TRY_FOR_CMD(cb);
	switch(type) {
	case TONE:
		start_new_helper_task(boost::shared_ptr<HelperTask>(
			new SendCommandTask(
				this,
				boost::bind(cb,_1),
				mfglibStopTone,
				EVENT_EMBER_CB_MFGLIB_STOP_TONE_RETURN
			)
		));
		break;
	case STREAM:
		start_new_helper_task(boost::shared_ptr<HelperTask>(
			new SendCommandTask(
				this,
				boost::bind(cb,_1),
				mfglibStopStream,
				EVENT_EMBER_CB_MFGLIB_STOP_STREAM_RETURN
			)
		));
		break;
	default:
		cb(kWPANTUNDStatus_InvalidArgument);
		return;
		break;
	}

	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_tx_packet(const Data& packet, int16_t repeat, CallbackWithStatus cb)
{
	BEGIN_TRY_FOR_CMD(cb);
	start_new_helper_task(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			boost::bind(mfglibSendPacket,Data(packet).data(), repeat),
			EVENT_EMBER_CB_MFGLIB_SEND_PACKET_RETURN
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_clockmon(bool enable, uint32_t timerId, CallbackWithStatus cb)
{
	BEGIN_TRY_FOR_CMD(cb);
	start_new_helper_task(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			boost::bind(emberSetClockMonitor,enable,timerId)
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_gpio_set(uint8_t port_pin, uint8_t config, uint8_t value, CallbackWithStatus cb)
{
	BEGIN_TRY_FOR_CMD(cb);
	start_new_helper_task(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			boost::bind(emberGpioCurrentConfig,port_pin, config, value),
			EVENT_EMBER_CB_GPIO_CURRENT_CONFIG_RETURN
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_gpio_get(uint8_t port_pin, CallbackWithStatusArg1 cb)
{
	BEGIN_TRY_FOR_CMD2(cb);
	start_new_helper_task(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			cb,
			boost::bind(emberGpioCurrentInput, port_pin),
			EVENT_EMBER_CB_GPIO_CURRENT_INPUT_RETURN,
			NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
			EmberCommands::PacketFieldType::EMBER_PACKED_UINT8_CHAR_T
		)
	));
	END_TRY_FOR_CMD2(cb);
}

void
EmberNCPControlInterface::mfg_channelcal(uint8_t channel, uint32_t duration,  CallbackWithStatus cb)
{
	BEGIN_TRY_FOR_CMD(cb);
	start_new_helper_task(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			boost::bind(mfglibTestContModCal, channel, duration)
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_channelcal_get(uint8_t channel, CallbackWithStatusArg1 cb)
{
	BEGIN_TRY_FOR_CMD2(cb);
	start_new_helper_task(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			cb,
			boost::bind(emberGetIndexedToken,(EmberTokenId)EMBER_CHANNEL_CAL_DATA_TOKEN,channel),
			CB_GET_CHANNEL_CAL_DATA_TOKEN_COMMAND_IDENTIFIER
		)
	));
	END_TRY_FOR_CMD2(cb);
}

void
EmberNCPControlInterface::handle_cb_mfg_rx(uint8_t *packet, uint8_t lqi, int8_t rssi)
{
	if (mMfgFirstPacketRSSI == -128) {
		mMfgFirstPacketRSSI = rssi;
		mMfgFirstPacketLQI = lqi;
	}

	(mOnMfgRXPacket)(Data(packet, packet[0]+1), lqi, rssi);
}

int
EmberNCPControlInterface::vprocess_mfg(int event, va_list args)
{
	uint8_t random_packet[257];
	uint8_t i = 0;
	static uint32_t seed = 0;
	static cms_t timer;

	// START OF PROTOTHREAD ===================================================
	EH_BEGIN_SUB(&mSubPT);

	// Initialize all of the diags state.
	mMfgFirstPacketRSSI = -128;
	mMfgFirstPacketLQI = 0;
	mMfgRepeatRandomTXInterval = MSEC_PER_SEC*4;
	mMfgRepeatRandomTXLen = 0;

	while (mMfgTestMode) {
		EH_YIELD();

		while (mMfgTestMode && (mMfgRepeatRandomTXLen > 0)) {

			timer = time_ms();

			if (mMfgRepeatRandomTXLen >= sizeof(random_packet)) {
			    mMfgRepeatRandomTXLen = sizeof(random_packet) - 1;
			}
			random_packet[0] = mMfgRepeatRandomTXLen;

			// Generate the random data for the packet.
			// We use a quick-and-dirty LCG random number
			// generator here because we don't care about
			// the quality of the random numbers. We don't
			// use `rand()` because Coverty complains, because
			// it can't tell the difference between a case
			// where using `rand()` is reasonable or not.
			// The values `1664525` and `1013904223` are
			// Knuth's suggested parameters for an LCG
			// with a period of 2^32. We only use the most
			// significant bits, as recommended.
			for (;i != mMfgRepeatRandomTXLen; i++) {
				seed = seed * 1664525 + 1013904223;
				random_packet[i+1] = static_cast<uint8_t>(seed >> 24);
			}

			syslog(LOG_NOTICE, "MFG-DIAGS: Sending random %d-byte packet. . .", mMfgRepeatRandomTXLen);

			mfglibSendPacket(random_packet, 0);

			CONTROL_REQUIRE_FLUSH_WITHIN(
				NCP_DEFAULT_COMMAND_SEND_TIMEOUT,
				on_error
			);

			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				event == EVENT_EMBER_CB_MFGLIB_SEND_PACKET_RETURN,
				on_error
			);

			timer = time_ms() - timer;

			if (timer < mMfgRepeatRandomTXInterval) {
				EH_SLEEP_FOR(
					(mMfgRepeatRandomTXInterval - timer) / (float)MSEC_PER_SEC
				);
			}
		}
	}

on_error:;

	// END OF PROTOTHREAD ===================================================
	EH_END();
}
