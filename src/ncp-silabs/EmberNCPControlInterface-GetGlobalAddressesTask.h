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
 *		Task to get global addresses from NCP.
 *
 */

#ifndef __wpantund__EmberNCPControlInterface_GetGlobalAddressesTask__
#define __wpantund__EmberNCPControlInterface_GetGlobalAddressesTask__

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"
#include "FirmwareInterface.h"

namespace nl {
namespace wpantund {

class nl::wpantund::EmberNCPControlInterface::GetGlobalAddressesTask : public HelperTask
{
public:

	GetGlobalAddressesTask(EmberNCPControlInterface* interface, CallbackWithStatusArg1 cb);

	virtual int vprocess_event(int event, va_list args);

private:
	std::list<std::string> mOutValue;
};

}; // namespace wpantund
}; // namespace nl



#endif /* defined(__wpantund__EmberNCPControlInterface_GetGlobalAddressesTask__) */
