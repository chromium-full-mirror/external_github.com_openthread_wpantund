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

#ifndef __wpantund__EmberNCPControlInterface_ScanTask__
#define __wpantund__EmberNCPControlInterface_ScanTask__

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"
#include "FirmwareInterface.h"

using namespace nl;
using namespace nl::wpantund;

namespace nl {
namespace wpantund {

class nl::wpantund::EmberNCPControlInterface::ScanTask : public HelperTask
{
public:
	ScanTask(
		EmberNCPControlInterface* interface,
		CallbackWithStatusArg1 cb,
		ScanType scan_type,
		int period,
		uint32_t channel_mask
	);
	virtual ~ScanTask() { finish(kWPANTUNDStatus_Canceled); }

	virtual void finish(int status, const boost::any& value = boost::any());

	virtual int vprocess_event(int event, va_list args);

private:
	ScanType mScanType;
	int mPeriod;
	uint32_t mChannelMask;
};

}; // namespace wpantund
}; // namespace nl


#endif /* defined(__wpantund__EmberNCPControlInterface_ScanTask__) */
