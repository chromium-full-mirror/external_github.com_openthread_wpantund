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
 *      Ember-style IPC method call serialization functions.
 *
 */


#ifndef __wpantund__EmberPacked__
#define __wpantund__EmberPacked__

#include <stdarg.h>
#include <boost/shared_ptr.hpp>

#include "Data.h"
#include "NetworkInstance.h"
#include "NCPControlInterface.h"

namespace nl {
namespace wpantund {
namespace EmberCommands {

#define EMBER_PACKED_INT8		"s"
#define EMBER_PACKED_UINT8		"u"
#define EMBER_PACKED_UINT16		"v"
#define EMBER_PACKED_UINT32		"w"
#define EMBER_PACKED_BLOCK		"b"

Data PackArguments(const char* pack_format, ...);
Data VPackArguments(const char* pack_format, va_list args);
int UnpackArguments(const Data& data, const char* pack_format, ...);
int UnpackArguments(const uint8_t *data_ptr, size_t data_len, const char* pack_format, ...);
int VUnpackArguments(const uint8_t *data_ptr, size_t data_len, const char* pack_format, va_list args);

class PacketFieldType {
public:
	enum Type {
		EMBER_PACKED_UNKNOWN_CHAR_T =     '0',
		EMBER_PACKED_INT8_CHAR_T =        's',
		EMBER_PACKED_UINT8_CHAR_T =       'u',
		EMBER_PACKED_UINT16_CHAR_T =      'v',
		EMBER_PACKED_UINT32_CHAR_T =      'w',
		EMBER_PACKED_BLOCK_CHAR_T =       'b'
	};
	Type t_;
	PacketFieldType(Type t) : t_(t) {}
	PacketFieldType(const char *p) : t_(EMBER_PACKED_UNKNOWN_CHAR_T) {
		if (*p == 's') { t_ = EMBER_PACKED_INT8_CHAR_T; }
		if (*p == 'u') { t_ = EMBER_PACKED_UINT8_CHAR_T; }
		if (*p == 'v') { t_ = EMBER_PACKED_UINT16_CHAR_T; }
		if (*p == 'w') { t_ = EMBER_PACKED_UINT32_CHAR_T; }
		if (*p == 'b') { t_ = EMBER_PACKED_BLOCK_CHAR_T; }
	}
	PacketFieldType() : t_(EMBER_PACKED_UNKNOWN_CHAR_T) {}

	operator Type () const {return t_;}
	operator bool () const {return t_ != EMBER_PACKED_UNKNOWN_CHAR_T;}

private:
	template<typename T>
	operator T () const;
};

}; // namespace EmberCommands
}; // namespace wpantund
}; // namespace nl


#endif /* defined(__wpantund__EmberPacked__) */
