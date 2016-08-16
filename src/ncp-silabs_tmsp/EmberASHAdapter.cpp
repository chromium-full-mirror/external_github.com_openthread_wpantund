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
 *      Implementation of the ASHv3 SocketAdapter.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"

#include "EmberASHAdapter.h"
#include <syslog.h>
#include "Data.h"
#include <errno.h>
#include <algorithm>

#include "EmberAPIGlue.h"

__BEGIN_DECLS
#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "hal/micro/micro.h"
#include "hal/micro/serial.h"
#include "hal/micro/generic/ash-v3.h"
#include "hal/micro/uart-link.h"
__END_DECLS

nl::wpantund::EmberASHAdapter* gASHAdapter;

__BEGIN_DECLS

//const char * emAppName = "wpantund";

void
emberAshStatusHandler(AshState state)
{
	gASHAdapter->mAshState = (int)state;
	syslog(LOG_DEBUG, "emberAshStatusHandler(): state = %d", (int)state);
}

void emLoadSerialTx()
{
//	syslog(LOG_NOTICE, "emLoadSerialTx()");
}


uint16_t serialCopyFromRx(const uint8_t *data, uint16_t length)
{
	if(length) {
		syslog(LOG_DEBUG, "Reading in %d bytes from ASH", (int)length);
		gASHAdapter->mRXBuffer.push(data, length);
		gASHAdapter->mDataInFlight = true;
	}
	return length;
}

void emResetSerialState(bool external)
{
	syslog(LOG_INFO, "emResetSerialState(): external=%d", external);
	if (external && gASHAdapter->mDataInFlight) {
		gASHAdapter->mDidReset = true;
		gASHAdapter->mRXBuffer.clear();
		gASHAdapter->mTXBuffer.clear();
		gASHAdapter->mTXErr = 0;
		gASHAdapter->mDataInFlight = false;
	}
}

void halHostUartLinkTx(const int8u *data, int16u length)
{
	if (gASHAdapter->get_parent()) {
		syslog(LOG_DEBUG, "Writing out %d bytes from ASH", (int)length);
		gASHAdapter->mTXBuffer.push(data, length);
	}
}

boolean halHostUartTxIdle(void)
{
	if (gASHAdapter->get_parent()) {
		return gASHAdapter->mTXBuffer.empty();
	}
	return false;
}

extern int16u halHostEnqueueTx(const int8u *data, int16u length);

__END_DECLS

// --------------------

using namespace nl::wpantund;

EmberASHAdapter::EmberASHAdapter(boost::shared_ptr<SocketWrapper> parent)
	:SocketAdapter(parent)
{
	ash_reset();

	mDidReset = false;
	mNeedsFlush = false;
	mWantsMoreData = false;
}

EmberASHAdapter::~EmberASHAdapter()
{
	gASHAdapter = NULL;
}

boost::shared_ptr<nl::SocketAdapter>
EmberASHAdapter::create(boost::shared_ptr<nl::SocketWrapper> parent)
{
	return boost::shared_ptr<SocketAdapter>(new EmberASHAdapter(parent));
}

const boost::shared_ptr<nl::SocketWrapper>&
EmberASHAdapter::set_parent(boost::shared_ptr<nl::SocketWrapper> parent)
{
	if (SocketAdapter::set_parent(parent)) {
		ash_reset();
	}
	return mParent;
}

void
EmberASHAdapter::ash_reset()
{
	gASHAdapter = this;
	mAshState = 0;
	mRXBuffer.clear();
	mTXBuffer.clear();
	mDidReset = true;
	mNeedsFlush = false;
	mTXErr = 0;

	if (mParent) {
		uartLinkReset();
	}

	mDataInFlight = false;
}

void
EmberASHAdapter::reset()
{
	syslog(LOG_INFO, "EmberASHAdapter::reset()");
	ash_reset();

	SocketAdapter::reset();
}

bool
EmberASHAdapter::did_reset()
{
	bool ret = SocketAdapter::did_reset();

	if (ret && mDataInFlight) {
		syslog(LOG_INFO, "EmberASHAdapter: Parent socket reset, resetting ASHv3...");
		ash_reset();
	}

	ret = mDidReset;

	mDidReset = false;

	return ret;
}

ssize_t
EmberASHAdapter::write(const void* data, size_t len)
{
	ssize_t ret = 0;

	// Don't allow anything to be written until
	// the reset condition has been acknowledged.
	require(!mDidReset, bail);

	if (gASHAdapter->mTXErr != 0) {
		ret = gASHAdapter->mTXErr;
		gASHAdapter->mTXErr = 0;
		goto bail;
	}

	if (len && can_write()) {
		ret = ::halHostEnqueueTx((const int8u *)data, len);

		if (ret == 0) {
			// If can_write() returned true but no data was accepted by ASH,
			// then something horrible has happened. Assert.
			assert(!can_write());
		}
	}

	if (ret > 0) {
		mNeedsFlush = true;
		mDataInFlight = true;
	}

bail:
	return ret;
}

ssize_t
EmberASHAdapter::handle_rx()
{
	uint8_t buffer[150];
	ssize_t buffer_len = sizeof(buffer);

	if (SocketAdapter::can_read()) {
		buffer_len = SocketAdapter::read((void*)buffer, buffer_len);
	} else {
		buffer_len = 0;
	}

	if (buffer_len < 0) {
		syslog(LOG_ERR, "EmberASHAdapter: Error on Read: %d", (int)buffer_len);
	} else if (buffer_len >= 0) {
		gASHAdapter = const_cast<EmberASHAdapter*>(this);
		::emProcessAshRxInput(buffer, buffer_len);
	}
	return buffer_len;
}

ssize_t
EmberASHAdapter::handle_tx(void)
{
	ssize_t written = 0;

	gASHAdapter = this;

	::uartFlushTx();
	mNeedsFlush = false;

	while (!mTXBuffer.empty() && SocketAdapter::can_write()) {
		written = SocketAdapter::write(
			(const void*)mTXBuffer.data_ptr(),
			mTXBuffer.size_of_data_ptr()
		);

		if (written < 0) {
			syslog(LOG_ERR, "EmberASHAdapter: Error on Write: %d", (int)written);
			mTXErr = (int)written;
			break;

		} else if (written > 0) {
			mTXBuffer.pop(written);

			if (mTXBuffer.empty()) {
				emAshNotifyTxComplete();
			}
		}
	}

	return written;
}

ssize_t
EmberASHAdapter::read(void* data, size_t len)
{
	ssize_t ret = 0;

	if (!mDidReset) {
		if (mRXBuffer.size() > 0) {
			if (len > mRXBuffer.size()) {
				len = mRXBuffer.size();
			}

			len = mRXBuffer.pull((uint8_t*)data, len);

			ret = len;
		} else {
			ret = 0;
		}
	}

	return ret;
}

bool
EmberASHAdapter::can_read(void)const
{
	return !mDidReset && !mRXBuffer.empty();
}

bool
EmberASHAdapter::can_write(void)const
{
	gASHAdapter = const_cast<EmberASHAdapter*>(this);
	return !mDidReset && (mAshState == ASH_STATE_RUNNING) && uartTxSpaceAvailable();
}

cms_t
EmberASHAdapter::get_ms_to_next_event(void)const
{
	cms_t ret = SocketAdapter::get_ms_to_next_event();

	if (mNeedsFlush) {
		ret = 0;
	} else {
		ret = std::min(EmberGlue::get_ms_to_next_ember_event(),ret);
	}

	return ret;
}

int
EmberASHAdapter::get_write_fd(void)const
{
	// We don't allow the writability of the FD to be used
	// for async I/O, since this socket is driven by software.
	return -1;
}

int
EmberASHAdapter::update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout)
{
	int fd;

	::uartFlushTx();
	mNeedsFlush = false;

	if ((write_fd_set != NULL) && !mTXBuffer.empty()) {
		fd = get_parent()->get_write_fd();
		FD_SET(fd, write_fd_set);
		FD_SET(fd, error_fd_set);
		if (max_fd != NULL) {
			*max_fd = std::max(*max_fd, fd);
		}
	}

	if (read_fd_set != NULL) {
		fd = get_parent()->get_read_fd();
		FD_SET(fd, read_fd_set);
		FD_SET(fd, error_fd_set);
		if (max_fd != NULL) {
			*max_fd = std::max(*max_fd, fd);
		}
	}

	return SocketAdapter::update_fd_set(read_fd_set, write_fd_set, error_fd_set, max_fd, timeout);
}

int
EmberASHAdapter::process(void)
{
	int ret = 0;

	gASHAdapter = this;
	EmberGlue::process_ember();

	require_noerr(SocketAdapter::process(), bail);

	require((ret = (int)handle_tx()) >= 0, bail);

	require((ret = (int)handle_rx()) >= 0, bail);

	require((ret = (int)handle_tx()) >= 0, bail);   // To ensure quick tx of ash acks

bail:
	// If ret wasn't an error, set it to zero.
	if (ret >= 0) {
		ret = 0;
	}

	return ret;
}
