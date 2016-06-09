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

#ifndef __wpantund__EmberNCPControlInterface_DeepSleepTask__
#define __wpantund__EmberNCPControlInterface_DeepSleepTask__

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"

using namespace nl;
using namespace nl::wpantund;

namespace nl {
namespace wpantund {

class EmberNCPControlInterface::DeepSleepTask : public HelperTask
{
public:
	DeepSleepTask(
		EmberNCPControlInterface* interface,
		CallbackWithStatusArg1 cb
	);
	virtual int vprocess_event(int event, va_list args);
	virtual void finish(int status, const boost::any& value = boost::any());
};

}; // namespace wpantund
}; // namespace nl


#endif /* defined(__wpantund__EmberNCPControlInterface_DeepSleepTask__) */
