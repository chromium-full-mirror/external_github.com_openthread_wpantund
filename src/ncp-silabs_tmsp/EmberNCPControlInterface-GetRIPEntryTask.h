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

#ifndef __wpantund__EmberNCPControlInterface_GetRIPEntryTask__
#define __wpantund__EmberNCPControlInterface_GetRIPEntryTask__

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"
#include "ValueMap.h"

namespace nl {
namespace wpantund {

class nl::wpantund::EmberNCPControlInterface::GetRIPEntryTask : public HelperTask
{
public:
	enum OutputMode  {
		kOutputModeString,
		kOutputModeValueMapList
	};

	GetRIPEntryTask(EmberNCPControlInterface* interface, CallbackWithStatusArg1 cb, OutputMode output_mode = kOutputModeString);

	virtual int vprocess_event(int event, va_list args);

private:
	std::string RipEntryToString(uint8_t index, const EmberRipEntry *ripEntry);
	ValueMap RipEntryToValueMap(uint8_t index, const EmberRipEntry *ripEntry);
	void add_to_output(uint8_t index, const EmberRipEntry *ripEntry);

	int mIndex;
	std::list<std::string> mStringOutputList;
	std::list<ValueMap> mValueMapOutputList;
	OutputMode mOutputMode;
};

}; // namespace wpantund
}; // namespace nl



#endif /* defined(__wpantund__EmberNCPControlInterface_GetRIPEntryTask__) */
