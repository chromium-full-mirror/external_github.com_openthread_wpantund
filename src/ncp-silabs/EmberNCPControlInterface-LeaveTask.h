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

#ifndef __wpantund__EmberNCPControlInterface_LeaveTask__
#define __wpantund__EmberNCPControlInterface_LeaveTask__

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"
#include "FirmwareInterface.h"

using namespace nl;
using namespace nl::wpantund;

namespace nl {
namespace wpantund {

class EmberNCPControlInterface::LeaveTask : public HelperTask
{
public:
	LeaveTask(
		EmberNCPControlInterface* interface,
		CallbackWithStatusArg1 cb
	);
	virtual int vprocess_event(int event, va_list args);
};

}; // namespace wpantund
}; // namespace nl


#endif /* defined(__wpantund__EmberNCPControlInterface_LeaveTask__) */
