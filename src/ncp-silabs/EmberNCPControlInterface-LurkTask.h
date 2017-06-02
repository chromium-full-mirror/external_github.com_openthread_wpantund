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

#ifndef __wpantund__EmberNCPControlInterface_LurkTask__
#define __wpantund__EmberNCPControlInterface_LurkTask__

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"
#include "FirmwareInterface.h"

namespace nl {
namespace wpantund {

#define EMBER_NCP_ENTER_SLEEP_TIMEOUT			0.5

class nl::wpantund::EmberNCPControlInterface::LurkTask : public HelperTask
{
public:

	LurkTask(EmberNCPControlInterface* interface, CallbackWithStatusArg1 cb);
	virtual ~LurkTask() { finish(kWPANTUNDStatus_Canceled); }

	virtual void finish(int status, const boost::any& value = boost::any());

	virtual int vprocess_event(int event, va_list args);

private:
	NCPState mOriginalState;
};

}; // namespace wpantund
}; // namespace nl



#endif /* defined(__wpantund__EmberNCPControlInterface_LurkTask__) */
