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

#ifndef __wpantund__EmberNCPControlInterface_FormTask__
#define __wpantund__EmberNCPControlInterface_FormTask__

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"

using namespace nl;
using namespace nl::wpantund;

namespace nl {
namespace wpantund {

class EmberNCPControlInterface::FormTask : public HelperTask
{
public:
	FormTask(
		EmberNCPControlInterface* interface,
		CallbackWithStatusArg1 cb,
	    const std::string& network_name,
		NodeType node_type,
		uint32_t channel_mask,
		const uint8_t *ula_prefix = NULL,
		const uint8_t *legacy_ula_prefix = NULL
	);
	virtual int vprocess_event(int event, va_list args);

private:
	EmberNetworkParameters mParameters;
	std::string mNetworkName;
	uint16_t mOptions;
	uint32_t mChannelMask;
};

}; // namespace wpantund
}; // namespace nl


#endif /* defined(__wpantund__EmberNCPControlInterface_FormTask__) */
