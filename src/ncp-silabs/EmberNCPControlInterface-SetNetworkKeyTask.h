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

#ifndef __wpantund__EmberNCPControlInterface_SetNetworkKey__
#define __wpantund__EmberNCPControlInterface_SetNetworkKey__

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"
#include "FirmwareInterface.h"

namespace nl {
namespace wpantund {

class nl::wpantund::EmberNCPControlInterface::SetNetworkKeyTask : public HelperTask
{
public:
	SetNetworkKeyTask(EmberNCPControlInterface* interface, CallbackWithStatusArg1 cb, const nl::Data& network_key, uint32_t network_key_index);

	virtual int vprocess_event(int event, va_list args);

private:

	nl::Data CMD_HostJoinClientComplete(void);
	nl::Data CMD_CommissionNetwork(void);

	nl::Data mNetworkKey;
	uint32_t mNetworkKeyIndex;

	nl::Data mCommissionNetworkData;
	nl::Data mJoinCommissionedData;
};

}; // namespace wpantund
}; // namespace nl

#endif /* defined(__wpantund__EmberNCPControlInterface_SetNetworkKey__) */
