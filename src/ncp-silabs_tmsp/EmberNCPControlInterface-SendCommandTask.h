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

#ifndef __wpantund__EmberNCPControlInterface_SendCommandTask__
#define __wpantund__EmberNCPControlInterface_SendCommandTask__

#include "EmberNCPControlInterface.h"
#include "EmberNCPInstance.h"
#include "EmberNCPUtils.h"
#include "EmberPacked.h"

namespace nl {
namespace wpantund {

class nl::wpantund::EmberNCPControlInterface::SendCommandTask : public HelperTask
{
public:

	SendCommandTask(EmberNCPControlInterface* interface,
		CallbackWithStatusArg1 cb,
		const Data& send_command,
		int callback_event = EVENT_NULL,
		int timeout = NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		EmberCommands::PacketFieldType ret_packed_type = EmberCommands::PacketFieldType::EMBER_PACKED_UNKNOWN_CHAR_T);

	SendCommandTask(EmberNCPControlInterface* interface,
		const CallbackWithStatusArg1& cb,
		const CallbackSimple& send_command_callback,
		int callback_event = EVENT_NULL,
		int timeout = NCP_DEFAULT_COMMAND_RESPONSE_TIMEOUT,
		EmberCommands::PacketFieldType ret_packed_type = EmberCommands::PacketFieldType::EMBER_PACKED_UNKNOWN_CHAR_T);

	virtual int vprocess_event(int event, va_list args);

private:
	int mCallbackEvent;
	int mTimeout;
	Data mSendCommand;
	CallbackSimple mSendCommandCallback;
	EmberCommands::PacketFieldType mRetPackedType;
};

}; // namespace wpantund
}; // namespace nl



#endif /* defined(__wpantund__EmberNCPControlInterface_SendCommandTask__) */
