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
#include "FirmwareInterface.h"

#include "CallbackStore.hpp"
#include "config-file.h"
#include "nlpt.h"
#include "string-utils.h"
#include "any-to.h"
#include "time-utils.h"
#include <syslog.h>

#include "EmberNCPControlInterface-SendCommandTask.h"
#include "EmberNCPControlInterface-MfgFinish.h"

using namespace nl;
using namespace wpantund;

void
EmberNCPControlInterface::mfg_finish(CallbackWithStatusArg1 cb)
{
	BEGIN_TRY_FOR_CMD2(cb);
	StartNewTask(boost::shared_ptr<HelperTask>(
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
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			mFirmwareInterface->CMD_MfgStart(type),
			mFirmwareInterface->GetIDFor_CBMfgStart()
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_end_test(uint8_t type, CallbackWithStatus cb)
{
	BEGIN_TRY_FOR_CMD(cb);
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			mFirmwareInterface->CMD_MfgStop(type),
			mFirmwareInterface->GetIDFor_CBMfgStop()
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_tx_packet(const Data& packet, int16_t repeat, CallbackWithStatus cb)
{
	BEGIN_TRY_FOR_CMD(cb);
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			mFirmwareInterface->CMD_MfgTXPacket(packet, repeat),
			mFirmwareInterface->GetIDFor_CBMfgSendPacket()
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_clockmon(bool enable, uint32_t timerId, CallbackWithStatus cb)
{
	BEGIN_TRY_FOR_CMD(cb);
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			mFirmwareInterface->CMD_MfgClockMon(enable, timerId)
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_gpio_set(uint8_t port_pin, uint8_t config, uint8_t value, CallbackWithStatus cb)
{
	BEGIN_TRY_FOR_CMD(cb);
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb,_1),
			mFirmwareInterface->CMD_GPIOSet(port_pin, config, value),
			mFirmwareInterface->GetIDFor_CBGPIOCurrentConfig()
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_gpio_get(uint8_t port_pin, CallbackWithStatusArg1 cb)
{
	BEGIN_TRY_FOR_CMD2(cb);
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			cb,
			mFirmwareInterface->CMD_GPIOGet(port_pin),
			mFirmwareInterface->GetIDFor_CBGPIOCurrentInput(),
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
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			boost::bind(cb, _1),
			mFirmwareInterface->CMD_ChannelCal(channel, duration)
		)
	));
	END_TRY_FOR_CMD(cb);
}

void
EmberNCPControlInterface::mfg_channelcal_get(uint8_t channel, CallbackWithStatusArg1 cb)
{
	BEGIN_TRY_FOR_CMD2(cb);
	StartNewTask(boost::shared_ptr<HelperTask>(
		new SendCommandTask(
			this,
			cb,
			mFirmwareInterface->CMD_ChannelCalGet(channel),
			mFirmwareInterface->GetIDFor_CBGetChannelCalToken()
		)
	));
	END_TRY_FOR_CMD2(cb);
}

void
EmberNCPControlInterface::handle_cb_mfg_rx(const Data& data)
{
	int packet_size = 0;
	const uint8_t* packet_data = NULL;
	int8_t rssi = 0;
	uint8_t lqi = 0;

	require(
		EmberCommands::UnpackArguments(
			data,
			EMBER_PACKED_UINT16 "bus",
			NULL, // Skip command identifier,
			&packet_size,
			&packet_data,
			&lqi,
			&rssi
		) != -1,
		bail
	);

	if (mMfgFirstPacketRSSI == -128) {
		mMfgFirstPacketRSSI = rssi;
		mMfgFirstPacketLQI = lqi;
	}

	(mOnMfgRXPacket)(Data(packet_data, packet_size), lqi, rssi);

bail:
	return;
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

			CONTROL_REQUIRE_SENT_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				mFirmwareInterface->CMD_MfgTXPacket(nl::Data(random_packet, mMfgRepeatRandomTXLen + 1), 0),
				on_error
			);

			EH_REQUIRE_WITHIN(
				NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
				event == mFirmwareInterface->GetIDFor_CBMfgSendPacket(),
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
