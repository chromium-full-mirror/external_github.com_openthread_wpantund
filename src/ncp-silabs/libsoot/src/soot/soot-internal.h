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
 *      Internal header file for the `Soot` asynchronous serial
 *      reliability layer
 *
 */

#ifndef __nl_soot_internal_header__
#define __nl_soot_internal_header__

#include "soot.h"
#include "soot-time.h"

// ----------------------------------------------------------------------------
// MARK: -
// MARK: Constants and Configuration Options

//! Work-around for implementations which don't always send us an RSTACK
#ifndef SOOT_CONFIG_MISSING_RSTACK_WORKAROUND
#define SOOT_CONFIG_MISSING_RSTACK_WORKAROUND       1
#endif

//! Work-around for implementations which sometimes send things out of order
#ifndef SOOT_CONFIG_HANDLE_OUT_OF_ORDER
#define SOOT_CONFIG_HANDLE_OUT_OF_ORDER             SOOT_EMBEDDED
#endif

//! Seed for packet corruption PRNG
#ifndef SOOT_CONFIG_DEBUG_STREAM_CORRUPT_SEED
#define SOOT_CONFIG_DEBUG_STREAM_CORRUPT_SEED       0x0000
#endif

#ifndef SOOT_CONFIG_ACK_TIMEOUT_MS
#define SOOT_CONFIG_ACK_TIMEOUT_MS                  (MSEC_PER_SEC/2)
#endif

#ifndef SOOT_CONFIG_MAX_RETRIES
#define SOOT_CONFIG_MAX_RETRIES                     16
#endif

#ifndef SOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_FACTOR
#define SOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_FACTOR  0
#endif

#ifndef SOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_WINDOW
#define SOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_WINDOW  100
#endif

#ifndef SOOT_CONFIG_DEBUG_TX_STREAM_CORRUPT_FACTOR
#define SOOT_CONFIG_DEBUG_TX_STREAM_CORRUPT_FACTOR  0
#endif

#ifndef SOOT_CONFIG_DEBUG_TX_STREAM_CORRUPT_WINDOW
#define SOOT_CONFIG_DEBUG_TX_STREAM_CORRUPT_WINDOW  100
#endif


// ----------------------------------------------------------------------------
// MARK: -
// MARK: Preprocessor Macros and Constants

#define SOOT_CNTRL_TYPE_MASK                   0xC0
#define SOOT_CNTRL_OUTGOING_FRAME_COUNTER_MASK 0x38
#define SOOT_CNTRL_ACK_NACK_FRAME_COUNTER_MASK 0x07

#define SOOT_HIGH_OCTET_NEEDS_ESCAPE_FLAG      0x80
#define SOOT_LOW_OCTET_NEEDS_ESCAPE_FLAG       0x40

#define SOOT_BYTE_FLAG             0x7E
#define SOOT_BYTE_ESC              0x7D
#define SOOT_BYTE_XON              0x11
#define SOOT_BYTE_XOFF             0x13
#define SOOT_BYTE_SPECIAL          0xF8

#define SOOT_ESCAPE_XFORM          0x20
#define SOOT_MAGIC_ESCAPE_BIT      0x10

#define SOOT_FRAME_COUNTER_MASK    0x07

#define SOOT_FRAME_INC(x)		   ((x)>=SOOT_FRAME_COUNTER_MASK?1:(x)+1)
#define SOOT_FRAME_DEC(x)		   ((x)<=1?SOOT_FRAME_COUNTER_MASK:(x)-1)

#define SOOT_ACK_TIMEOUT_MS        (cms_t)(SOOT_CONFIG_ACK_TIMEOUT_MS)

#define SOOT_HEADER_SIZE           4
#define SOOT_CRC_SIZE              3
#define SOOT_MAX_PAYLOAD_SIZE      (SOOT_MAX_PACKET_SIZE-SOOT_HEADER_SIZE-SOOT_CRC_SIZE)

#ifdef __GNUC__
#define NOT_USED_ATTR              __attribute__ ((unused))
#else
#define NOT_USED_ATTR
#endif

// ----------------------------------------------------------------------------
// MARK: -
// MARK: Headers

#include "pt.h"
#include "dp.h"

// ----------------------------------------------------------------------------
// MARK: -
// MARK: Data Structures and Enumerations

typedef enum {
	SOOT_STATE_INIT,
	SOOT_STATE_RESET,
	SOOT_STATE_RUNNING,
} soot_state_t;

typedef enum {
	SOOT_PACKET_TYPE_RESET,
	SOOT_PACKET_TYPE_RSTACK,
	SOOT_PACKET_TYPE_ACK,
	SOOT_PACKET_TYPE_NACK,
} soot_packet_type_t;

//! TX Buffer State Enumeration
/*!	TX Buffers have the following states:
**
**	* `SOOT_TX_BUFFER_STATE_READY`
**		* Transitions to `flushing` state when data is inserted
**	* `SOOT_TX_BUFFER_STATE_FLUSHING`
**		* If we have finished flushing the buffer...
**			* ...and the payload length is zero: Go directly to
**	          acknowledged state
**			* ...and the payload length is nonzero: Go directly to
**	          waiting state
**	* `SOOT_TX_BUFFER_STATE_WAITING`
**		* Transitions to acknowledged state once packet with matching
**	      AFC is received.
**		* After timer expiration, returns to flushing state
**	* `SOOT_TX_BUFFER_STATE_ACKNOWLEGED`
**		* Can transition back to `FLUSHING` to send new ACK/NACK, but
**	      keeps same OFC.
**		* Can transition back to `READY`, but OFC must be changed to
**	      `NextOutboundFrame` and `NextOutboundFrame` incremented
*/
typedef enum {
	SOOT_TX_BUFFER_STATE_READY,
	SOOT_TX_BUFFER_STATE_FLUSHING,
	SOOT_TX_BUFFER_STATE_WAITING,
	SOOT_TX_BUFFER_STATE_ACKNOWLEGED,
} soot_tx_buffer_state_t;

typedef struct {
	soot_tx_buffer_state_t mState;
	int mRetries;
	cms_t mNextAttempt;
	uint8_t mBytesSent;
	uint8_t mLen;
	uint8_t mOFC:3,mAFC:3,mType:2;

	union {
		uint8_t mRaw[SOOT_MAX_PACKET_SIZE];
		struct {
			uint8_t mAshFlag;
			uint8_t mHeaderEscape;
			uint8_t mCtrl;
			uint8_t mPayloadLen;
			uint8_t mPayload[SOOT_MAX_PAYLOAD_SIZE+SOOT_CRC_SIZE];
		} mFrame;
	} mData;

} soot_tx_buffer_t;

struct soot_instance_internals {
#if !SOOT_EMBEDDED
	void*                 mCallbackContext;
	soot_tx_callback_func mCallbackTXFunc;
	soot_read_callback_func mCallbackReadFunc;
#endif

	soot_state_t          mSootState;

	soot_tx_buffer_t      mTXBuffers[2];
	soot_tx_buffer_t      *mHotBuffer;

	struct dp             mRXDataDP;

	uint8_t               mRXHdrEsc;
	uint8_t               mRXType:2, mRXOFC:3, mRXAFC:3;
	uint8_t               mRXLen;
	uint8_t               mRXDataBuffer[SOOT_MAX_PAYLOAD_SIZE];
	uint16_t              mRXCRC;
	uint16_t              mRXCalcCRC;

	uint8_t               mNextRXOFC:3,mDidReset:1;
	uint8_t               mLastAckedOutboundFrame:3,
	                      mNextOutboundFrame:3;

#if SOOT_CONFIG_HANDLE_OUT_OF_ORDER
	uint8_t               mOOORXLen;
	uint8_t               mOOORXDataBuffer[SOOT_MAX_PAYLOAD_SIZE];
#endif
};


// ----------------------------------------------------------------------------
// MARK: -
// MARK: Embedded Optimizations

#if SOOT_EMBEDDED
soot_instance_t gSootGlobalInstance;
#define soot_init(self)                             soot_init()
#define soot_finalize(self)                         soot_finalize()
#define soot_current_tx_buffer(self)                soot_current_tx_buffer()
#define soot_prev_tx_buffer(self)                   soot_prev_tx_buffer()
#define soot_next_writable_tx_buffer(self)          soot_next_writable_tx_buffer()
#define soot_next_transmitable_tx_buffer(self)      soot_next_transmitable_tx_buffer()
#define soot_signal_nack(self)                      soot_signal_nack()
#define soot_signal_ack(self)                       soot_signal_ack()
#define soot_handle_rx_reset(self)                  soot_handle_rx_reset()
#define soot_handle_rx_packet_afc(self)             soot_handle_rx_packet_afc()
#define soot_handle_rx_packet(self)                 soot_handle_rx_packet()
#define soot_outbound_frame_recycle(self,...)       soot_outbound_frame_recycle(__VA_ARGS__)
#define soot_outbound_frame_did_ack(self,...)       soot_outbound_frame_did_ack(__VA_ARGS__)
#define soot_outbound_frame_flush(self,...)         soot_outbound_frame_flush(__VA_ARGS__)
#define soot_outbound_frame_did_flush(self,...)     soot_outbound_frame_did_flush(__VA_ARGS__)
#define soot_rx_data_parser(self,...) soot_rx_data_parser(__VA_ARGS__)
#define soot_hook_tx_data(self,...) soot_hook_tx_data(__VA_ARGS__)
#define soot_hook_read_data(self,...) soot_hook_read_data(__VA_ARGS__)
#define SOOT_EMBEDDED_SELF_HOOK \
	NOT_USED_ATTR static soot_instance_t* const self = &gSootGlobalInstance
#else
#define SOOT_EMBEDDED_SELF_HOOK
#endif

#endif
