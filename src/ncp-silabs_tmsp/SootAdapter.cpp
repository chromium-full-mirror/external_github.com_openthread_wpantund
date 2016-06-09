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
 *      Implementation of the LibSoot SocketAdapter.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"

#include "SootAdapter.h"
#include <syslog.h>
#include "Data.h"
#include <errno.h>
#include <algorithm>

// --------------------

using namespace nl;

static ssize_t
soot_adapter_tx_callback(void* tx_data_context, const void *buf, size_t nbyte)
{
	SootAdapter* wrapper = static_cast<SootAdapter*>(tx_data_context);

	return wrapper->get_parent()->write(buf, nbyte);
}

static ssize_t
soot_adapter_read_callback(void* read_data_context, const void *buf, size_t nbyte)
{
	SootAdapter* wrapper = static_cast<SootAdapter*>(read_data_context);

	if (wrapper->mRXBuffer.space_available() >= nbyte) {
		wrapper->mRXBuffer.push(static_cast<const uint8_t*>(buf), nbyte);
		return nbyte;
	}

	return 0;
}

SootAdapter::SootAdapter(boost::shared_ptr<SocketWrapper> parent)
	:SocketAdapter(parent), mRXBuffer()
{
	mSootInstance = soot_create();

	soot_set_callbacks(
		mSootInstance,
		&soot_adapter_tx_callback,
		&soot_adapter_read_callback,
		this
	);
}

SootAdapter::~SootAdapter()
{
	soot_release(mSootInstance);
}

boost::shared_ptr<nl::SocketAdapter>
SootAdapter::create(boost::shared_ptr<nl::SocketWrapper> parent)
{
	return boost::shared_ptr<SocketAdapter>(new SootAdapter(parent));
}

const boost::shared_ptr<nl::SocketWrapper>&
SootAdapter::set_parent(boost::shared_ptr<nl::SocketWrapper> parent)
{
	if (SocketAdapter::set_parent(parent)) {
		if (soot_is_connected(mSootInstance)) {
			soot_reset(mSootInstance);
		}
	}
	return mParent;
}

void
SootAdapter::reset()
{
	syslog(LOG_INFO, "SootAdapter::reset()");
	SocketAdapter::reset();
	soot_reset(mSootInstance);
}

bool
SootAdapter::did_reset()
{
	bool ret = SocketAdapter::did_reset();

	if (ret) {
		if (!soot_did_reset(mSootInstance)) {
			soot_reset(mSootInstance);
			ret = soot_did_reset(mSootInstance);
		}
	} else {
		ret = soot_did_reset(mSootInstance);
	}


	return ret;
}

ssize_t
SootAdapter::write(const void* data, size_t len)
{
	ssize_t ret = 0;

	ret = soot_write(mSootInstance, data, len);

	return ret;
}

ssize_t
SootAdapter::handle_rx()
{
	uint8_t buffer[SOOT_MAX_PACKET_SIZE*3];
	ssize_t buffer_len = sizeof(buffer);

	if (SocketAdapter::can_read()) {
		buffer_len = SocketAdapter::read((void*)buffer, buffer_len);
		assert(buffer_len != 0);
	} else {
		buffer_len = 0;
	}

	require(buffer_len >= 0, bail);
	soot_process_rx_data(mSootInstance, buffer, buffer_len);

bail:
	if (buffer_len < 0) {
		syslog(LOG_ERR, "SootAdapter: Error on Read: %d %s", (int)buffer_len, strerror((int)-buffer_len));
	}
	return buffer_len;
}


ssize_t
SootAdapter::read(void* data, size_t len)
{
    ssize_t ret = 0;

    if (soot_get_last_status(mSootInstance) != SOOT_STATUS_RESET) {
		ret = mRXBuffer.pull(static_cast<uint8_t*>(data), len);
    }

    return ret;
}

bool
SootAdapter::can_read(void)const
{
	return (soot_get_last_status(mSootInstance) != SOOT_STATUS_RESET) && !mRXBuffer.empty();
}

bool
SootAdapter::can_write(void)const
{
	return soot_can_write(mSootInstance);
}

cms_t
SootAdapter::get_ms_to_next_event(void)const
{
	cms_t ret = SocketAdapter::get_ms_to_next_event();

	if (can_read() && (ret > MSEC_PER_SEC/2)) {
		ret = MSEC_PER_SEC/2;
	}

	ret = std::min(ret, soot_get_timeout(mSootInstance));

	return ret;
}

int
SootAdapter::get_write_fd(void)const
{
	// We don't allow the writability of the FD to be used
	// for async I/O, since this socket is driven by software.
	return -1;
}

int
SootAdapter::update_fd_set(fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout)
{
	if ((write_fd_set != NULL) && soot_wants_to_tx_data(mSootInstance)) {
		FD_SET(get_parent()->get_write_fd(), write_fd_set);
		FD_SET(get_parent()->get_write_fd(), error_fd_set);
	}

	if (read_fd_set != NULL) {
		FD_SET(get_parent()->get_read_fd(), read_fd_set);
		FD_SET(get_parent()->get_read_fd(), error_fd_set);
	}

	return SocketAdapter::update_fd_set(read_fd_set, write_fd_set, error_fd_set, max_fd, timeout);
}

int
SootAdapter::process(void)
{
	int ret = SOOT_STATUS_OK;

	require_noerr(ret = SocketAdapter::process(), bail);

	if (soot_wants_to_tx_data(mSootInstance) && get_parent()->can_write()) {
		soot_process_tx_data(mSootInstance);

		ret = soot_get_last_status(mSootInstance);
		require((ret == SOOT_STATUS_OK) || (ret == SOOT_STATUS_RESET), bail);
	}


	require_noerr((ret = (handle_rx()>=0?SOOT_STATUS_OK:SOOT_STATUS_FAILURE)), bail);

	if (soot_wants_to_tx_data(mSootInstance) && get_parent()->can_write()) {
		soot_process_tx_data(mSootInstance);

		ret = soot_get_last_status(mSootInstance);
		require((ret == SOOT_STATUS_OK) || (ret == SOOT_STATUS_RESET), bail);
	}

bail:
	if (ret == SOOT_STATUS_RESET) {
		ret = SOOT_STATUS_OK;
	}

	__ASSERT_MACROS_check(ret == SOOT_STATUS_OK);

	return ret;
}
