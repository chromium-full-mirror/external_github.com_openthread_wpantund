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
 *      Header file for the `Soot` asynchronous serial reliability layer
 *
 */

#ifndef __nl_soot_header__
#define __nl_soot_header__

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

__BEGIN_DECLS

// ----------------------------------------------------------------------------

#ifndef DOXYGEN_SHOULD_SKIP_THIS
# if defined(__GNUC__) && !SOOT_EMBEDDED
#  define SOOT_API_EXTERN              extern __attribute__ ((visibility ("default")))
#  define SOOT_API_NONNULL_ALL         __attribute__((nonnull))
#  define SOOT_API_WARN_UNUSED_RESULT  __attribute__((warn_unused_result))
# endif // ifdef __GNUC__
#endif // ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifndef SOOT_API_EXTERN
# define SOOT_API_EXTERN               extern
#endif

#ifndef SOOT_HOOK_EXTERN
# define SOOT_HOOK_EXTERN              extern
#endif

#ifndef SOOT_API_NONNULL_ALL
# define SOOT_API_NONNULL_ALL
#endif

#ifndef SOOT_API_WARN_UNUSED_RESULT
# define SOOT_API_WARN_UNUSED_RESULT
#endif

// ----------------------------------------------------------------------------

#define SOOT_MAX_PACKET_SIZE       64

typedef enum {
	SOOT_STATUS_OK,         //!< Operation has completed successfully.
	SOOT_STATUS_FAILURE,    //!< Operation has failed for some undefined reason.
	SOOT_STATUS_RESET,      //!< A state reset has been induced.
} soot_status_t;

typedef ssize_t (*soot_tx_callback_func)(void* tx_data_context, const void *buf, size_t nbyte);
typedef ssize_t (*soot_read_callback_func)(void* read_data_context, const void *buf, size_t nbyte);

struct soot_instance_internals;

typedef struct soot_instance_internals soot_instance_t;

// ----------------------------------------------------------------------------

#if SOOT_EMBEDDED
//! On embedded systems, we know we will always only have
//! a single libsoot instance, so we can save a considerable
//! amount of stack space by simply removing the first argument
//! from many functions. In order to make things as maintainable
//! as possible, these macros do all of the work for us.
SOOT_API_EXTERN soot_instance_t        gSootGlobalInstance;
#define soot_release(self)             soot_release()
#define soot_is_connected(self)        soot_is_connected()
#define soot_wants_to_tx_data(self)    soot_wants_to_tx_data()
#define soot_process_tx_data(self)     soot_process_tx_data()
#define soot_get_timeout(self)         soot_get_timeout()
#define soot_can_write(self)           soot_can_write()
#define soot_reset(self)               soot_reset()
#define soot_did_reset(self)           soot_did_reset()
#define soot_get_last_status(self)     soot_get_last_status()
#define soot_process_rx_data(self,...) soot_process_rx_data(__VA_ARGS__)
#define soot_write(self,...)           soot_write(__VA_ARGS__)
#endif

// ----------------------------------------------------------------------------
// MARK: Embedded Hooks

#if SOOT_EMBEDDED || defined(DOXYGEN_SHOULD_SKIP_THIS)

//! Hook to handle data that needs to be transmitted.
/*!	This function returns the actual number of bytes transmitted, or
**	negative-one if there is an error
**
**	@note Used only when `SOOT_EMBEDDED` is set.
*/
SOOT_HOOK_EXTERN ssize_t soot_hook_tx_data(const void *buf, size_t nbyte)
	SOOT_API_WARN_UNUSED_RESULT
	SOOT_API_NONNULL_ALL;

//! Hook to handle received data.
/*!	This function must only return the exact value of `nbyte`, `0`, or `-1`.
**	 * Returns `nbyte` when the bytes were consumed successfully.
**	 * Returns `0` or `-1` when the bytes were not consumed (pointer remains
**	   valid)
**
**	Returning a positive value that is note `nbyte` is not supported.
**
**	@note Used only when `SOOT_EMBEDDED` is set.
*/
SOOT_HOOK_EXTERN ssize_t soot_hook_read_data(const void *buf, size_t nbyte)
	SOOT_API_WARN_UNUSED_RESULT
	SOOT_API_NONNULL_ALL;

//! Hook to get the current time in milliseconds, modulo 2^32
/*!	@note Used only when `SOOT_EMBEDDED` is set.
*/
SOOT_HOOK_EXTERN int32_t soot_hook_get_ms(void)
	SOOT_API_WARN_UNUSED_RESULT;

#endif

// ----------------------------------------------------------------------------

#if !SOOT_EMBEDDED || defined(DOXYGEN_SHOULD_SKIP_THIS)

//! Sets the transmit callback function.
/*!	@note Not available if `SOOT_EMBEDDED` is set. */
SOOT_API_EXTERN void soot_set_callbacks(
	soot_instance_t* instance,
	soot_tx_callback_func tx_callback,
	soot_read_callback_func read_callback,
	void* context
);

#endif // !SOOT_EMBEDDED

// ----------------------------------------------------------------------------

//!	Allocate and initialize a soot instance.
/*!
**	When libsoot is compiled with SOOT_EMBEDDED, this function
**	initializes the global instance and returns a pointer to it.
*/
SOOT_API_EXTERN soot_instance_t* soot_create(void);

//!	Finalize and deallocate a soot instance.
/*!
**	When libsoot is compiled with SOOT_EMBEDDED, this function
**	simply finalizes the global instance.
*/
SOOT_API_EXTERN void soot_release(soot_instance_t* instance)
	SOOT_API_NONNULL_ALL;

//! Returns true if the reliable connection has been established.
SOOT_API_EXTERN bool soot_is_connected(soot_instance_t* instance)
	SOOT_API_WARN_UNUSED_RESULT
	SOOT_API_NONNULL_ALL;

//! Called when raw data is received from serial port.
/*!
**	It is from this function that the read_data callback
**	will be executed.
*/
SOOT_API_EXTERN void soot_process_rx_data(soot_instance_t* instance, const void *buf, size_t nbyte)
	SOOT_API_NONNULL_ALL;

//! Returns true if the instance needs soot_process_tx_data() to be called soon.
/*!
**	Should be checked after calls to `soot_process_rx_data()` and
**	`soot_process_tx_data()`.
*/
SOOT_API_EXTERN bool soot_wants_to_tx_data(const soot_instance_t* instance)
	SOOT_API_WARN_UNUSED_RESULT
	SOOT_API_NONNULL_ALL;

//! Called when soot should attempt to send data using the tx_data() callback.
/*!
**	It is from this function that the tx_data callback
**	will be executed.
*/
SOOT_API_EXTERN void soot_process_tx_data(soot_instance_t* instance)
	SOOT_API_NONNULL_ALL;

//! Maximum amount of time (in ms) before soot_process_tx_data() should be called.
/*!
**	If the function returns a value of zero, `soot_process_tx_data()`
**	should be called as soon as possible.
**
**	Should be checked after calls to `soot_process_rx_data()` and
**	`soot_process_tx_data()`.
*/
SOOT_API_EXTERN int32_t soot_get_timeout(const soot_instance_t* instance)
	SOOT_API_WARN_UNUSED_RESULT
	SOOT_API_NONNULL_ALL;

// ----------------------------------------------------------------------------

//! Returns true if a call to `soot_write()` could successfully write some data.
SOOT_API_EXTERN bool soot_can_write(const soot_instance_t* instance)
	SOOT_API_WARN_UNUSED_RESULT
	SOOT_API_NONNULL_ALL;

//! Writes data to encapsulated stream
/*!
**	@return Returns the number of bytes actually written, or -1 upon error.
*/
SOOT_API_EXTERN ssize_t soot_write(soot_instance_t* instance, const void *buf, size_t nbyte)
	SOOT_API_WARN_UNUSED_RESULT
	SOOT_API_NONNULL_ALL;

//! Reset the connection
/*!
**	Immediately after calling this function, any calls to
**	`soot_is_connected()` will return `false` until the
**	instance resynchronizes with the remote device.
**
**	If previously connected, this function will cause
**	the instance to enter a reset state, which must be
**	cleared by calling `soot_did_reset()`.
*/
SOOT_API_EXTERN void soot_reset(soot_instance_t* instance)
	SOOT_API_NONNULL_ALL;

//! Check and clear the reset condition
/*!
**	Note that calling this function clears the reset condition.
**	If you want to check for a reset without clearing the condition,
**	use `soot_get_last_status()`.
*/
SOOT_API_EXTERN bool soot_did_reset(soot_instance_t* instance)
	SOOT_API_NONNULL_ALL;

//! Returns the last error status.
/*!
**	@sa soot_status_t
*/
SOOT_API_EXTERN soot_status_t soot_get_last_status(const soot_instance_t* instance)
	SOOT_API_WARN_UNUSED_RESULT
	SOOT_API_NONNULL_ALL;

// ----------------------------------------------------------------------------

__END_DECLS

#endif /* defined(__nl_soot_header__) */
