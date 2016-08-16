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
 *      Declaration of the LibSoot SocketAdapter.
 *
 */

#ifndef __wpantund__SootAdapter__
#define __wpantund__SootAdapter__

#include "SocketAdapter.h"
#include "Data.h"
#include "time-utils.h"
#include <boost/shared_ptr.hpp>
#include <soot/soot.h>
#include "RingBuffer.h"

namespace nl {

class SootAdapter : public SocketAdapter {
protected:
	SootAdapter(boost::shared_ptr<SocketWrapper> parent);

public:
	static boost::shared_ptr<SocketAdapter> create(boost::shared_ptr<SocketWrapper> parent = boost::shared_ptr<SocketWrapper>());

	virtual ~SootAdapter();
	virtual ssize_t write(const void* data, size_t len);
	virtual ssize_t read(void* data, size_t len);
	virtual bool can_read(void)const;
	virtual bool can_write(void)const;
	virtual int get_write_fd(void)const;
	virtual int process(void);
	virtual const boost::shared_ptr<SocketWrapper>& set_parent(boost::shared_ptr<SocketWrapper> parent);
	virtual cms_t get_ms_to_next_event(void)const;

	virtual void reset();
	virtual bool did_reset();

	virtual int update_fd_set(
		fd_set *read_fd_set,
		fd_set *write_fd_set,
		fd_set *error_fd_set,
		int *max_fd,
		cms_t *timeout
	);

private:
	void ash_reset();

public:
	ssize_t handle_rx(void);
	ssize_t handle_tx(void);

public:

	soot_instance_t *mSootInstance;

	RingBuffer<uint8_t, 512> mRXBuffer;
}; // class SootAdapter

}; // namespace nl

#endif /* defined(__wpantund__SootAdapter__) */
