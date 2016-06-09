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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"
#include "EmberPacked.h"
#include <syslog.h>
#include <errno.h>

using namespace nl;
using namespace wpantund;
using namespace EmberCommands;

Data
EmberCommands::PackArguments(const char* pack_format, ...)
{
	Data ret;
	va_list args;
	va_start(args, pack_format);

	ret = VPackArguments(pack_format, args);

	va_end(args);
	return ret;
}

Data
EmberCommands::VPackArguments(const char* pack_format, va_list args)
{
	Data ret;

	for (;*pack_format != 0; pack_format++) {
		EmberCommands::PacketFieldType fieldType(pack_format);

		if (fieldType == EmberCommands::PacketFieldType::EMBER_PACKED_INT8_CHAR_T) {
			int8_t arg = va_arg(args, int);
			ret.append(reinterpret_cast<const uint8_t*>(&arg), sizeof(arg));
		} else if (fieldType == EmberCommands::PacketFieldType::EMBER_PACKED_UINT8_CHAR_T) {
			uint8_t arg = va_arg(args, unsigned int);
			ret.append(reinterpret_cast<const uint8_t*>(&arg), sizeof(arg));
		} else if (fieldType == EmberCommands::PacketFieldType::EMBER_PACKED_UINT16_CHAR_T) {
			uint16_t arg = htons(va_arg(args, unsigned int));
			ret.append(reinterpret_cast<const uint8_t*>(&arg), sizeof(arg));
		} else if (fieldType == EmberCommands::PacketFieldType::EMBER_PACKED_UINT32_CHAR_T) {
			uint32_t arg = htonl(va_arg(args, uint32_t));
			ret.append(reinterpret_cast<const uint8_t*>(&arg), sizeof(arg));
		} else if (fieldType == EmberCommands::PacketFieldType::EMBER_PACKED_BLOCK_CHAR_T) {
			unsigned int block_len = va_arg(args, unsigned int);
			const uint8_t* arg = va_arg(args, const uint8_t*);
			uint8_t len_byte = static_cast<uint8_t>(block_len);

			require_action(block_len <= 255, bail, (ret = -1, errno = EOVERFLOW));

			ret.append(&len_byte, 1);
			ret.append(arg, block_len);
		} else {
			goto bail;
		}
	}

bail:
	return ret;
}

int
EmberCommands::UnpackArguments(const Data& data, const char* pack_format, ...)
{
	int ret;
	va_list args;
	va_start(args, pack_format);
	const uint8_t* data_ptr = data.data();
	size_t data_len = data.size();

	ret = VUnpackArguments(data_ptr, data_len, pack_format, args);

	va_end(args);
	return ret;
}

int
EmberCommands::UnpackArguments(const uint8_t *data_ptr, size_t data_len, const char* pack_format, ...)
{
	int ret;
	va_list args;
	va_start(args, pack_format);

	ret = VUnpackArguments(data_ptr, data_len, pack_format, args);

	va_end(args);
	return ret;
}

int
EmberCommands::VUnpackArguments(const uint8_t *data_ptr, size_t data_len, const char* pack_format, va_list args)
{
	int ret = 0;

	for (; *pack_format != 0; pack_format++) {
		void* arg_ptr = va_arg(args, void*);
		EmberCommands::PacketFieldType fieldType(pack_format);

		if (fieldType == EmberCommands::PacketFieldType::EMBER_PACKED_INT8_CHAR_T) {
			require_action(data_len >= 1, bail, (ret = -1, errno = EOVERFLOW));

			if (arg_ptr != NULL) {
				*static_cast<int8_t*>(arg_ptr) = *data_ptr;
			}

			data_ptr++;
			data_len--;
			ret++;
		} else if (fieldType == EmberCommands::PacketFieldType::EMBER_PACKED_UINT8_CHAR_T) {
			require_action(data_len >= 1, bail, (ret = -1, errno = EOVERFLOW));

			if (arg_ptr != NULL) {
				*static_cast<uint8_t*>(arg_ptr) = *data_ptr;
			}

			data_ptr++;
			data_len--;
			ret++;
		} else if (fieldType == EmberCommands::PacketFieldType::EMBER_PACKED_UINT16_CHAR_T) {
			require_action(data_len >= 2, bail, (ret = -1, errno = EOVERFLOW));

			if (arg_ptr != NULL) {
				*static_cast<uint16_t*>(arg_ptr) = (data_ptr[0] << 8) | data_ptr[1];
			}

			data_ptr += 2;
			data_len -= 2;
			ret += 2;
		} else if (fieldType == EmberCommands::PacketFieldType::EMBER_PACKED_UINT32_CHAR_T) {
			require_action(data_len >= 4, bail, (ret = -1, errno = EOVERFLOW));

			if (arg_ptr != NULL) {
				*static_cast<uint32_t*>(arg_ptr) = (data_ptr[0] << 24) | (data_ptr[1] << 16) | (data_ptr[2] << 8) | data_ptr[3];
			}

			data_ptr += 4;
			data_len -= 4;
			ret += 4;
		} else if (fieldType == EmberCommands::PacketFieldType::EMBER_PACKED_BLOCK_CHAR_T) {
			require_action(data_len >= 1, bail, (ret = -1, errno = EOVERFLOW));

			require_action(data_len >= 1 + data_ptr[0], bail, (ret = -1, errno = EOVERFLOW));

			if (arg_ptr != NULL) {
				*static_cast<int*>(arg_ptr) = data_ptr[0];
			}

			arg_ptr = va_arg(args, void*);

			if ((data_ptr[0] > 0) && (arg_ptr != NULL)) {
				*static_cast<const void**>(arg_ptr) = reinterpret_cast<const void*>(&data_ptr[1]);
			}

			data_len -= 1 + data_ptr[0];
			ret += 1 + data_ptr[0];
			data_ptr += 1 + data_ptr[0];
		} else {
			assert_printf("Bad format character '%c' (%d)", *pack_format, *pack_format);
			errno = EINVAL;
			ret = -1;
			goto bail;
		}
	}

bail:
	return ret;
}
