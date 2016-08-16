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

#ifndef __wpantund__EmberNCPControlInterface_JoinHelper__
#define __wpantund__EmberNCPControlInterface_JoinHelper__

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"
#include "FirmwareInterface.h"

namespace nl {
namespace wpantund {

class nl::wpantund::EmberNCPControlInterface::JoinTask : public HelperTask
{
public:
	JoinTask(EmberNCPControlInterface* interface, CallbackWithStatusArg1 cb, const WPAN::NetworkInstance& network, NodeType node_type, const uint8_t prefix[8] = NULL);

	virtual int vprocess_event(int event, va_list args);

private:
	WPAN::NetworkInstance mNetwork;
	NodeType mNodeType;
	uint8_t mPrefix[8];
};

}; // namespace wpantund
}; // namespace nl

#endif /* defined(__wpantund__EmberNCPControlInterface_JoinHelper__) */
