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

#ifndef __wpantund__EmberNCPInstance__
#define __wpantund__EmberNCPInstance__

#include "NCPInstanceBase.h"
#include "EmberNCPControlInterface.h"
#include "CallbackStore.hpp"
#include "nlpt.h"
#include "SocketWrapper.h"
#include "SocketAsyncOp.h"

#include <queue>
#include <set>
#include <map>
#include <errno.h>

enum {
	MUX_TYPE_MANAGEMENT = 1,
	MUX_TYPE_IPV6_DATA = 2,
	MUX_TYPE_INSECURE_DATA = 3,
	MUX_TYPE_LEGACY_DATA = 4
};

#define EMBER_MUX_TYPE_IS_IPV6_DATA(x)    ( \
                      ((x) == MUX_TYPE_IPV6_DATA) \
		           || ((x) == MUX_TYPE_INSECURE_DATA) \
		           || ((x) == MUX_TYPE_LEGACY_DATA) \
				)

enum {
	MGNT_MSG_CMD        = 1,
	MGNT_MSG_STDERR = 2,
	MGNT_MSG_RESPONSE_DONE  = 3,
	MGNT_MSG_RESPONSE_ERROR = 4,
};


#define MAX_PACKET_SIZE     2000
#define NCP_FRAME_OVERHEAD  4

#define MAX_NEARBY_NETWORKS     16

struct ncp_frame_s {
	char preamble;            // Always '['
	uint8_t type;
	uint16_t length;
	uint8_t data[0];
};

WPANTUND_DECLARE_NCPINSTANCE_PLUGIN(silabs, EmberNCPInstance);

namespace nl {
namespace wpantund {

class EmberNCPInstance;
class EmberNCPControlInterface;

class EmberNCPInstance : public NCPInstanceBase {
	friend class EmberNCPControlInterface;

public:

	//! Type for outbound data queue, including completion callback.
	typedef std::queue<std::pair< Data, CallbackWithStatus > > queue_type;

public:
	EmberNCPInstance(const Settings& settings = Settings());

	virtual ~EmberNCPInstance();

	virtual EmberNCPControlInterface& get_control_interface();
	virtual cms_t get_ms_to_next_event();

public:
	static bool setup_property_supported_by_class(const std::string& prop_name);

	virtual void reinitialize_ncp(void);

protected:

	virtual void hard_reset_ncp(void);
	virtual int set_ncp_power(bool power);

	virtual void reset_tasks(wpantund_status_t status = kWPANTUNDStatus_Canceled);
	virtual bool is_busy(void);

	virtual std::set<std::string> get_supported_property_keys()const;

private:
	virtual void address_was_added(const struct in6_addr& addr, int prefix_len);
	virtual void address_was_removed(const struct in6_addr& addr, int prefix_len);

	int send_management_command(const Data& data, const boost::function<void(int)>& cb=NilReturn());
	void flush_queued_commands(int error=kWPANTUNDStatus_Canceled);
	bool outbound_command_is_in_flight(void);
	bool inbound_command_is_in_flight(void);

	virtual void handle_ncp_state_change(NCPState new_ncp_state, NCPState old_ncp_state);

	NLPT_THREAD(ncp_to_driver_pump());
	NLPT_THREAD(driver_to_ncp_pump());

	virtual int vprocess_event(int event, va_list args);


private:
	EmberNCPControlInterface mControlInterface;

	time_t mInboundAbortTime;
	ncp_frame_s mInboundFrame;
	uint8_t mInboundBuffer[MAX_PACKET_SIZE + 4];
	ssize_t mInboundBufferLen;

	queue_type mOutboundQueue;
	uint8_t mOutboundBuffer[MAX_PACKET_SIZE + NCP_FRAME_OVERHEAD];
	ssize_t mOutboundBufferLen;
	ssize_t mOutboundBufferSent;
	boost::function<void(int)> mOutboundCallback;

}; // class EmberNCPInstance
}; // namespace wpantund
}; // namespace nl

#endif /* defined(__wpantund__EmberNCPInstance__) */
