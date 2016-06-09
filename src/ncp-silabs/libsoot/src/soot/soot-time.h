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
 *      Timing Definitions
 *
 */

#ifndef soot_soot_time_h
#define soot_soot_time_h

#include <stdint.h>

typedef int32_t cms_t;

#ifndef MSEC_PER_SEC
#define MSEC_PER_SEC            1000
#endif

#ifndef CMS_DISTANT_FUTURE
#define CMS_DISTANT_FUTURE      (cms_t)INT32_MAX
#endif

//! Returns the number of milliseconds that have passed since time `x`
#define CMS_SINCE(x)            (cms_t)(soot_hook_get_ms() - (x))

#if !SOOT_EMBEDDED && !defined(SOOT_CONFIG_HOOK_GET_MS)
#include <sys/time.h>
#include <sys/cdefs.h>
#include <time.h>
static int32_t
soot_hook_get_ms(void)
{
#if HAVE_CLOCK_GETTIME
	static const int32_t kNSEC_PER_MSEC = 1000000;
	struct timespec tv = { 0 };
	int ret;

	ret = clock_gettime(CLOCK_MONOTONIC, &tv);

	return (cms_t)(tv.tv_sec * MSEC_PER_SEC)
		+ (cms_t)(tv.tv_nsec / kNSEC_PER_MSEC);
#else
	static const int32_t kUSEC_PER_MSEC = 1000;
	struct timeval tv = { 0, 0 };
	gettimeofday(&tv, NULL);
	return (int32_t)(tv.tv_sec * MSEC_PER_SEC)
		+ (int32_t)(tv.tv_usec / kUSEC_PER_MSEC);
#endif
}
#elif defined(SOOT_CONFIG_HOOK_GET_MS)
#define soot_hook_get_ms()	(SOOT_CONFIG_HOOK_GET_MS)
#endif

#endif
