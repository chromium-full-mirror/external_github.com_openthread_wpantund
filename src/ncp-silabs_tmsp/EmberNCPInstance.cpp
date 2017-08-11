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
 *      This file implements the code which handles the management of
 *      ember-style NCPs.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "EmberNCPInstance.h"
#include "time-utils.h"
#include "assert-macros.h"
#include <syslog.h>
#include <errno.h>
#include "socket-utils.h"
#include <stdexcept>
#include <sys/file.h>
#include "SuperSocket.h"
#include "EmberNCPUtils.h"
#include "SootAdapter.h"
#include "EmberASHAdapter.h"

__BEGIN_DECLS
#include PLATFORM_HEADER
#include "stack/include/ember.h"
__END_DECLS

#include "EmberAPIGlue.h"

#define MUX_TYPE_TO_CSTR(x) \
			       (((x) == MUX_TYPE_INSECURE_DATA) \
					? "INSECURE" \
					: ((x) == MUX_TYPE_LEGACY_DATA) \
						? "LEGACY" \
						: "SECURE")

using namespace nl;
using namespace wpantund;

WPANTUND_DEFINE_NCPINSTANCE_PLUGIN(silabs_tmsp, EmberNCPInstance);

#ifndef USE_EMBER_API_TO_PARSE_STREAM
#define USE_EMBER_API_TO_PARSE_STREAM 1
#endif

class EmberNCPInstance* nl::wpantund::gNCPInstance;

EmberNCPInstance::EmberNCPInstance(const Settings& settings) :
	NCPInstanceBase(settings),
	mInboundAbortTime(0),
	mInboundFrame(),
	mInboundBufferLen(0)
{
	boost::shared_ptr<SocketAdapter> socket_adapter;
	mControlInterface.mNCPInstance = this;

	assert(gNCPInstance == NULL);
	gNCPInstance = this;
	mOutboundBufferLen = 0;
	mOutboundBufferSent = 0;

	EmberGlue::init();

	// If this socket name is a device, chances are it will need libsoot.
	// This can be overridden via the configuration file or the command line.
	if (settings.count(kWPANTUNDProperty_ConfigNCPSocketPath)
	 && settings.count(kWPANTUNDProperty_ConfigNCPReliabilityLayer) == 0
	) {
		std::string ncp_socket_name = settings.find(kWPANTUNDProperty_ConfigNCPSocketPath)->second;
		if (socket_name_is_device(ncp_socket_name.c_str())) {
			socket_adapter = EmberASHAdapter::create();
			//socket_adapter = SootAdapter::create();
		}
		syslog(LOG_INFO, "Setting `%s` was not explicitly set, defaulting to `%s`", kWPANTUNDProperty_ConfigNCPReliabilityLayer, socket_name_is_device(ncp_socket_name.c_str())?"ashv3":"none");
		syslog(LOG_INFO, "...because `%s` looks like it %s a device.", ncp_socket_name.c_str(), socket_name_is_device(ncp_socket_name.c_str())?"is":"is NOT");
	}

	if (!settings.empty()) {
		int status;
		Settings::const_iterator iter;

		for(iter = settings.begin(); iter != settings.end(); iter++) {
			if (strcaseequal(iter->first.c_str(), kWPANTUNDProperty_ConfigNCPReliabilityLayer)) {
				if (strcaseequal(iter->second.c_str(), "ashv3")) {
					socket_adapter = EmberASHAdapter::create();
				} else if (strcaseequal(iter->second.c_str(), "libsoot")) {
					socket_adapter = SootAdapter::create();
				} else if (iter->second == "none" || iter->second.empty()) {
					// Do nothing.
				} else {
					syslog(LOG_ERR, "Unrecognised reliability layer \"%s\"", iter->second.c_str());
				}
			} else if (!NCPInstanceBase::setup_property_supported_by_class(iter->first)) {
				status = static_cast<NCPControlInterface&>(get_control_interface())
					.property_set_value(iter->first, iter->second);

				if (status != 0) {
					syslog(LOG_WARNING, "Attempt to set property \"%s\" failed with err %d", iter->first.c_str(), status);
				}
			}
		}
	}

	set_socket_adapter(socket_adapter);
}

EmberNCPInstance::~EmberNCPInstance()
{
}

bool
EmberNCPInstance::setup_property_supported_by_class(const std::string& prop_name)
{
	return strcaseequal(prop_name.c_str(), kWPANTUNDProperty_ConfigNCPReliabilityLayer)
		|| NCPInstanceBase::setup_property_supported_by_class(prop_name);
}

int
EmberNCPInstance::set_ncp_power(bool power)
{
	int ret(NCPInstanceBase::set_ncp_power(power));

	if (ret >= 0) {
		flush_queued_commands();
		mInboundAbortTime = 0;
	}

	return ret;
}

EmberNCPControlInterface&
EmberNCPInstance::get_control_interface()
{
	return mControlInterface;
}

void
EmberNCPInstance::add_unicast_address_on_ncp(const struct in6_addr &addr, uint8_t prefix_len, CallbackWithStatus cb)
{
	if (!IN6_IS_ADDR_LINKLOCAL(&addr)) {
		mControlInterface.request_manual_address_from_ncp(addr, prefix_len);
	}
	cb(kWPANTUNDStatus_Ok);
}

void
EmberNCPInstance::remove_unicast_address_on_ncp(const struct in6_addr &addr, uint8_t prefix_len, CallbackWithStatus cb)
{
	mControlInterface.abandon_address_from_ncp(addr, prefix_len);
	cb(kWPANTUNDStatus_Ok);
}

void
EmberNCPInstance::add_multicast_address_on_ncp(const struct in6_addr &addr, CallbackWithStatus cb)
{
	cb(kWPANTUNDStatus_Ok);
}

void
EmberNCPInstance::remove_multicast_address_on_ncp(const struct in6_addr &addr, CallbackWithStatus cb)
{
	cb(kWPANTUNDStatus_Ok);
}

void
EmberNCPInstance::add_on_mesh_prefix_on_ncp(const struct in6_addr &addr, uint8_t prefix_len, uint8_t flags, bool stable,
					CallbackWithStatus cb)
{
	cb(kWPANTUNDStatus_Ok);
}

void
EmberNCPInstance::remove_on_mesh_prefix_on_ncp(const struct in6_addr &addr, uint8_t prefix_len, uint8_t flags,
					bool stable, CallbackWithStatus cb)
{
	cb(kWPANTUNDStatus_Ok);
}

int
EmberNCPInstance::send_management_command(const Data& data, const boost::function<void(int)>& cb)
{
	if (ncp_state_is_detached_from_ncp(get_ncp_state())) {
		return kWPANTUNDStatus_InvalidForCurrentState;
	}

	if (!data.empty()) {
		if (ncp_state_is_sleeping(get_ncp_state())) {
			set_ncp_power(true);
		}
	}

	mOutboundQueue.push(std::pair<Data, boost::function<void(int)> >(data, cb));

	return kWPANTUNDStatus_Ok;
}

void
EmberNCPInstance::flush_queued_commands(int error)
{
	queue_type queueCopy(mOutboundQueue);
	mOutboundQueue = queue_type();
	while(!queueCopy.empty()) {
		queueCopy.front().second(error);
		queueCopy.pop();
	}
}

void
EmberNCPInstance::reset_tasks(wpantund_status_t status)
{
	mControlInterface.reset_callbacks(status);
}

bool
EmberNCPInstance::outbound_command_is_in_flight(void)
{
	const ncp_frame_s *outbound_frame = reinterpret_cast<const struct ncp_frame_s *>(mOutboundBuffer);

	return !mOutboundQueue.empty()
		|| (mOutboundBufferLen != 0 && (outbound_frame->data[0] == MGNT_MSG_CMD));
}

bool
EmberNCPInstance::inbound_command_is_in_flight(void)
{
	// If the inbound timeout timer is active then we know there is an inbound
	// packet in-flight
	return mInboundAbortTime != 0;
}

cms_t
EmberNCPInstance::get_ms_to_next_event()
{
	cms_t tmp, cms = NCPInstanceBase::get_ms_to_next_event();

	tmp = mControlInterface.get_ms_to_next_event();
	if (tmp < cms) {
		cms = tmp;
	}

	if (!ncp_state_is_detached_from_ncp(get_ncp_state())) {
		tmp = EmberGlue::get_ms_to_next_ember_event();
		if (tmp < cms) {
			cms = tmp;
		}
	}

	return cms;
}

bool
EmberNCPInstance::is_busy(void)
{
	bool ret = NCPInstanceBase::is_busy();

	ret |= !mControlInterface.is_ok_to_sleep();

	return ret;
}

void
EmberNCPInstance::reinitialize_ncp(void)
{
	NCPInstanceBase::reinitialize_ncp();
	mControlInterface.reinitialize_ncp();
}


std::set<std::string>
EmberNCPInstance::get_supported_property_keys()const
{
	return mControlInterface.get_supported_property_keys();
}

void
EmberNCPInstance::handle_ncp_state_change(NCPState new_ncp_state, NCPState old_ncp_state)
{
	NCPInstanceBase::handle_ncp_state_change(new_ncp_state, old_ncp_state);
	mControlInterface.handle_ncp_state_change(new_ncp_state, old_ncp_state);
}

char
EmberNCPInstance::ncp_to_driver_pump()
{
	struct nlpt*const pt = &mNCPToDriverPumpPT;
	IpModemReadStatus ip_modem_read_status;
	ssize_t bytes_read;
	uint16_t index;

	if (ncp_state_is_detached_from_ncp(get_ncp_state())) {
		// Do not do anything in this function if we are detached.
		return 0;
	}

	// Automatically detect socket resets and behave accordingly.
	if (mSerialAdapter->did_reset()) {
		syslog(LOG_NOTICE, "[-NCP-]: Socket Reset");
		NLPT_INIT(&mNCPToDriverPumpPT);
		NLPT_INIT(&mDriverToNCPPumpPT);

		mControlInterface.process_event(EVENT_NCP_CONN_RESET);
	}

	// Automatic timeout logic.
	if ((pt->pt.lc != 0)
	 && (mInboundAbortTime != 0)
	 && (mInboundAbortTime < time_get_monotonic())
	 && !mSerialAdapter->can_read()
	) {
		syslog(LOG_ERR, "[NCP->]: Send timeout! (flow-control problem?) pt@%d", (int)pt->pt.lc);
		NLPT_INIT(pt);
	}

	NLPT_BEGIN(pt);

	// This macro abstracts the logic to read a single character into
	// `data`, in a protothreads-friendly way.
#define READ_CHARACTER(pt, data, on_fail) \
	while(1) {                                  \
		NLPT_WAIT_UNTIL_READABLE_OR_COND(pt, mSerialAdapter->get_read_fd(), mSerialAdapter->can_read()); \
		ssize_t retlen = mSerialAdapter->read(data, 1); \
		if (retlen < 0) { \
			syslog(LOG_ERR, "[-NCP-]: Socket error on read: %s %d", \
			       strerror((int)-retlen), (int)(-retlen)); \
			signal_fatal_error(ERRORCODE_ERRNO); \
			goto on_fail; \
		} else if (retlen == 0) { \
			continue; \
		} \
		break ; \
	};


#if USE_EMBER_API_TO_PARSE_STREAM
	// Ignore any initial garbage.
	do {
		READ_CHARACTER(pt, (void*)&mInboundEmberStream.buffer[0], on_error);
	} while ('[' != mInboundEmberStream.buffer[0]);
	mInboundEmberStream.index = 1;
#endif

	while (!ncp_state_is_detached_from_ncp(get_ncp_state())) {

#if USE_EMBER_API_TO_PARSE_STREAM
		// This version of the parser largely just uses the SiLabs parser.
		// This is less efficient, but at least we are using their API.

		if (sizeof(mInboundEmberStream.buffer) <= mInboundEmberStream.index) {
			syslog(LOG_ERR, "[-NCP-]: Host stream buffer overflow");
			mControlInterface.process_event(EVENT_NCP_CONN_RESET);
			goto on_error;
		}

		// Yield until the socket is readable. We do a
		// yield here instead of a wait because we want to
		// only handle one packet per run through the main loop.
		// Using `YIELD` instead of `WAIT` guarantees that we
		// will yield control of the protothread at least once,
		// even if the socket is already readable.
		NLPT_WAIT_UNTIL_READABLE_OR_COND(pt, mSerialAdapter->get_read_fd(), mSerialAdapter->can_read());

		bytes_read = mSerialAdapter->read(
			mInboundEmberStream.buffer + mInboundEmberStream.index,
			sizeof(mInboundEmberStream.buffer) - mInboundEmberStream.index
		);

		if (bytes_read < 0) {
			pt->last_errno = errno;
			syslog(LOG_ERR, "[-NCP-]: Socket error on read: %s", strerror(pt->last_errno));
			errno = pt->last_errno;
			signal_fatal_error(ERRORCODE_ERRNO);
			goto on_error;
		}

		mInboundEmberStream.index += bytes_read;

		while (mInboundEmberStream.index >= UART_LINK_HEADER_SIZE) {
			index = mInboundEmberStream.index;

			ip_modem_read_status = processIpModemInput(
				&mInboundEmberStream,
				&EmberGlue::process_message_from_ncp
			);

			syslog(LOG_DEBUG, "processIpModemInput: %d", ip_modem_read_status);

			if (ip_modem_read_status == IP_MODEM_READ_FORMAT_ERROR) {
				syslog(LOG_ERR, "[-NCP-]: Format error on read");
				mControlInterface.process_event(EVENT_NCP_CONN_RESET);
				goto on_error;
			}

			if (mInboundEmberStream.index >= index) {
				// Break out of the loop if we haven't made any progress.
				break;
			}
		}

		NLPT_YIELD_UNTIL_READABLE_OR_COND(pt, mSerialAdapter->get_read_fd(), mSerialAdapter->can_read());

#else //#if USE_EMBER_API_TO_PARSE_STREAM

		// This is our original protothreads-based stream parser.
		// Kept around un case we have problems with the EmberAPI version.

		// ================================================================
		// Step 1: Wait for the preamble character.
		// ================================================================

		// No timeout while waiting for preamble character.
		mInboundAbortTime = 0;

		// Yield until the socket is readable. We do a
		// yield here instead of a wait because we want to
		// only handle one packet per run through the main loop.
		// Using `YIELD` instead of `WAIT` guarantees that we
		// will yield control of the protothread at least once,
		// even if the socket is already readable.
		NLPT_YIELD_UNTIL_READABLE_OR_COND(pt, mSerialAdapter->get_read_fd(), mSerialAdapter->can_read());

		do {
			READ_CHARACTER(pt, (void*)&mInboundFrame.preamble, on_error);

			if ('[' != mInboundFrame.preamble) {
				// The dreaded extraneous character error.

				// Log the error.
				{
					char printable = mInboundFrame.preamble;
					if(iscntrl(printable) || printable<0)
						printable = '.';

					syslog(LOG_WARNING,
						   "[NCP->] Extraneous Character: 0x%02X [%c] (%d)\n",
						   (uint8_t)mInboundFrame.preamble,
						   printable,
						   (uint8_t)mInboundFrame.preamble);
				}

				// Flush out all remaining data since this is a strong
				// indication that something has gone horribly wrong.
				while (mSerialAdapter->can_read()) {
					READ_CHARACTER(pt, (void*)&mInboundFrame.preamble, on_error);
				}

				ncp_is_misbehaving();
				goto on_error;
			}
		} while ('[' != mInboundFrame.preamble);



		// ================================================================
		// Step 2: Fetch the rest of the header.
		// ================================================================

		// Reset timeout to two seconds in the future.
		mInboundAbortTime = time_get_monotonic() + 2;

		// Read the rest of the frame header, one byte at a time.
		READ_CHARACTER(pt, (void*)((char*)&mInboundFrame+1), on_error);
		READ_CHARACTER(pt, (void*)((char*)&mInboundFrame+2), on_error);
		READ_CHARACTER(pt, (void*)((char*)&mInboundFrame+3), on_error);

		mInboundFrame.length = ntohs(mInboundFrame.length);

		// Make sure our packet is a sane length.
		require(mInboundFrame.length > 1, on_error);
		require(mInboundFrame.length <= MAX_PACKET_SIZE, on_error);

		// ================================================================
		// Step 3: Fetch the rest of the packet.
		// ================================================================

		// Set a timeout for the read.
		mInboundAbortTime = time_get_monotonic() + 5;

		// Read the rest of the packet.
		NLPT_ASYNC_READ_STREAM(
			pt,
			mSerialAdapter.get(),
			mInboundBuffer,
			mInboundFrame.length
		);
		mInboundBufferLen = pt->byte_count;

		if (pt->last_errno) {
			syslog(LOG_ERR, "[-NCP-]: Socket error on read: %s", strerror(pt->last_errno));
			errno = pt->last_errno;
			signal_fatal_error(ERRORCODE_ERRNO);
			goto on_error;
		}

		assert(mInboundBufferLen >= 0);

		// ================================================================
		// Step 4: Handle the inbound frame, dispatching callbacks, etc.
		// ================================================================

		EmberGlue::process_message_from_ncp(
			(SerialLinkMessageType)mInboundFrame.type,
			mInboundBuffer,
			mInboundFrame.length
		);

		// ================================================================
		// Step 5: Loop around and start over at Step 1.
		// ================================================================

#endif //#if !USE_EMBER_API_TO_PARSE_STREAM

	} // while(!ncp_state_is_detached_from_ncp(get_ncp_state()))

on_error:;
	// If we get here, we will restart the protothread at the next iteration.

	NLPT_END(pt);
}

char
EmberNCPInstance::driver_to_ncp_pump()
{
	struct nlpt*const pt = &mDriverToNCPPumpPT;
	ncp_frame_s *outbound_frame = (struct ncp_frame_s *)mOutboundBuffer;

	if (ncp_state_is_detached_from_ncp(get_ncp_state())) {
		// Do not do anything in this function if we are detached.
		return 0;
	}

	NLPT_BEGIN(pt);

	while (!ncp_state_is_detached_from_ncp(get_ncp_state())) {
		// If there is an outbound callback at this
		// point, then we assume it is stale and
		// immediately clear it out.
		if (!mOutboundCallback.empty()) {
			mOutboundCallback(kWPANTUNDStatus_Canceled);
			mOutboundCallback.clear();
		}

		// Wait for a packet to be available from interface OR management queue.
		if (!mOutboundQueue.empty()) {
			// If there is something in the outbound queue,
			// we shouldn't try any of the checks below, since it
			// will delay processing.

		} else if (static_cast<bool>(mLegacyInterface) && is_legacy_interface_enabled()) {
			NLPT_YIELD_UNTIL_READABLE2_OR_COND(
				pt,
				mPrimaryInterface->get_read_fd(),
				mLegacyInterface->get_read_fd(),
				mOutboundQueue.size()
				|| mLegacyInterface->can_read()
				|| mPrimaryInterface->can_read()
			);

		} else {
			NLPT_YIELD_UNTIL_READABLE_OR_COND(
				pt,
				mPrimaryInterface->get_read_fd(),
				mPrimaryInterface->can_read() || mOutboundQueue.size()
			);
		}

		// Get packet or management command, and also
		// perform any necessary filtering.
		if (mOutboundQueue.size()) {
			// We've got an outbound management command queued up.

			Data& next_packet = mOutboundQueue.front().first;
			mOutboundCallback = mOutboundQueue.front().second;

			if (next_packet.empty()) {
				mOutboundQueue.pop();
				mOutboundCallback(kWPANTUNDStatus_Ok);
				mOutboundCallback.clear();
				continue;
			}

			mOutboundBufferLen = next_packet.size();
			outbound_frame->data[0] = MGNT_MSG_CMD;
			outbound_frame->data[1] = 0;
			outbound_frame->data[2] = 0;
			memcpy((void*)(outbound_frame->data + 1),
			       next_packet.data(), mOutboundBufferLen);
			outbound_frame->type = UART_LINK_TYPE_MANAGEMENT;
			mOutboundBufferLen++;

			syslog(LOG_INFO,
			       "[->NCP] Command: %s length: %d\n",
					EmberGlue::describe_frame(next_packet.data(), next_packet.size()).c_str(),
			       (int)next_packet.size());

			mOutboundQueue.pop();
		} else {
			// There is an IPv6 packet waiting on one of the tunnel interfaces.

			if (mPrimaryInterface->can_read()) {
				mOutboundBufferLen = mPrimaryInterface->read(
					(void*)outbound_frame->data,
					MAX_PACKET_SIZE
				);
				outbound_frame->type = UART_LINK_TYPE_THREAD_DATA;
			} else if (static_cast<bool>(mLegacyInterface)) {
				mOutboundBufferLen = mLegacyInterface->read(
					(void*)outbound_frame->data,
					MAX_PACKET_SIZE
				);
				outbound_frame->type = UART_LINK_TYPE_ALT_DATA;
			}

			if (mOutboundBufferLen == 0) {
				// No packet...?
				continue;
			}

			if (0 > mOutboundBufferLen) {
				syslog(LOG_ERR,
				       "driver_to_ncp_pump: Socket error on read: %s",
				       strerror(errno));
				signal_fatal_error(ERRORCODE_ERRNO);
				break;
			}

			if (!should_forward_ncpbound_frame(&outbound_frame->type, outbound_frame->data, mOutboundBufferLen)) {
				continue;
			}
		}

#if VERBOSE_DEBUG
		// Very verbose debugging. Dumps out all outbound packets.
		if (mOutboundBufferLen + NCP_FRAME_OVERHEAD >= 5) {
			char readable_buffer[MAX_PACKET_SIZE];
			encode_data_into_string(mOutboundBuffer + 5,
			                        mOutboundBufferLen + NCP_FRAME_OVERHEAD - 5,
			                        readable_buffer,
			                        sizeof(readable_buffer),
			                        0);
			syslog(LOG_DEBUG, "\t↳ %s", (const char*)readable_buffer);
		}
#endif // VERBOSE_DEBUG

		// Frame the packet.
		outbound_frame->preamble = '[';
		outbound_frame->length = htons(mOutboundBufferLen);
		mOutboundBufferLen += NCP_FRAME_OVERHEAD;
		mOutboundBufferSent = 0;

		// Go ahead send
		NLPT_ASYNC_WRITE_STREAM(
			pt,
			mSerialAdapter.get(),
			mOutboundBuffer,
			mOutboundBufferLen
		);
		mOutboundBufferSent += pt->byte_count;

		mOutboundBufferLen = 0;

		require(pt->last_errno == 0, on_error);

		// Go ahead and fire off the "did send" callback.
		if (!mOutboundCallback.empty()) {
			mOutboundCallback(kWPANTUNDStatus_Ok);
			mOutboundCallback.clear();
		}

	} // while(true)

on_error:
	// If we get here, we will restart the protothread at the next iteration.

	if (!mOutboundCallback.empty()) {
		mOutboundCallback(kWPANTUNDStatus_Failure);
		mOutboundCallback.clear();
	}

	NLPT_END(pt);
}

int
EmberNCPInstance::vprocess_event(int event, va_list args)
{
	EH_BEGIN();

	EH_SPAWN(&mControlInterface.mControlPT, mControlInterface.vprocess_event(event, args));

	EH_END();
}
