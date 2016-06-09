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

#ifndef __wpantund__EmberNCPControlInterface_GetCounter__
#define __wpantund__EmberNCPControlInterface_GetCounter__

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"
#include "EmberNCPUtils.h"

namespace nl {
namespace wpantund {

class nl::wpantund::EmberNCPControlInterface::GetCounter : public HelperTask
{
public:

	GetCounter(EmberNCPControlInterface* interface, CallbackWithStatusArg1 cb, int counter_index);

	virtual int vprocess_event(int event, va_list args);

private:
	int mCounterIndex;
};

}; // namespace wpantund
}; // namespace nl



#endif /* defined(__wpantund__EmberNCPControlInterface_GetCounter__) */
