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
 *      Debug and logging stuff
 *
 */

#ifndef soot_soot_debug_h
#define soot_soot_debug_h

#include "assert-macros.h"

#if ASSERT_MACROS_USE_SYSLOG

# define ERROR_PRINTF(...) do { syslog(LOG_ERR, __VA_ARGS__); } while(0)
# define WARNING_PRINTF(...) do { syslog(LOG_WARNING, __VA_ARGS__); } while(0)

# if DEBUG
#  define DEBUG_PRINTF(...) do { syslog(LOG_DEBUG, __VA_ARGS__); } while(0)
# else // !DEBUG
#  define DEBUG_PRINTF(...) do {  } while(0)
# endif // if/else DEBUG

#else // !ASSERT_MACROS_USE_SYSLOG

# define ERROR_PRINTF(...) do { fprintf(assert_error_stream, __VA_ARGS__); fputc('\n', assert_error_stream); } while(0)
# if DEBUG
#  define WARNING_PRINTF(...) do { fprintf(assert_error_stream, __VA_ARGS__); fputc('\n', assert_error_stream); } while(0)
#  define DEBUG_PRINTF(...) do { fprintf(assert_error_stream, __VA_ARGS__); fputc('\n', assert_error_stream); } while(0)
# else // !DEBUG
#  define WARNING_PRINTF(...) do {  } while(0)
#  define DEBUG_PRINTF(...) do {  } while(0)
# endif // if/else DEBUG

#endif // if/else ASSERT_MACROS_USE_SYSLOG


#if VERBOSE_DEBUG
# define DEBUG_BYTES_PER_LINE			16
# define DEBUG_DUMP(ptr, desc, data, len) do { \
		const uint8_t* __xbyte = (uint8_t*)data; \
		ssize_t i = 0; \
		while(i<(len)) { \
			ssize_t j = 0; \
			char dump_string[DEBUG_BYTES_PER_LINE*3+1]; \
			for (j = 0;i<(len) && j<DEBUG_BYTES_PER_LINE;i++,j++) { \
				sprintf(dump_string+j*3, "%02X ", __xbyte[i]); \
			} \
			DEBUG_PRINTF("soot:%p: %s %3d Byte(s) :   %s%s",ptr, desc, (int)j, dump_string, (i<(len))?" ...":""); \
		}\
	} while(0)
#else // !VERBOSE_DEBUG
# define DEBUG_DUMP(ptr, desc, data, len) do {  } while(0)
#endif // if/else VERBOSE_DEBUG


#endif
