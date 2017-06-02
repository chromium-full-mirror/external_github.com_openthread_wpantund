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
 *      Implementation of `Soot` asynchronous serial reliability layer
 *
 */

// ----------------------------------------------------------------------------
// MARK: -
// MARK: Headers

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "assert-macros.h"

#include "soot-internal.h"
#include "soot-debug.h"

#include <string.h>

#if !SOOT_EMBEDDED
#include <stdlib.h>
#endif


// ----------------------------------------------------------------------------
// MARK: -
// MARK: Utility and Debugging functions

// **** LCOV_EXCL_START ****

#if SOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_FACTOR || SOOT_CONFIG_DEBUG_TX_STREAM_CORRUPT_FACTOR
static uint32_t
clz32(uint32_t x)
{
#ifdef __GNUC__
	return (uint32_t)__builtin_clz(x);
#else
	// Brain-dead naive version, to avoid licensing issues.
	// We can get away with this because it is only used for testing.
	// See <http://goo.gl/1P3aMZ> for a much faster version.
	int i;
	uint32_t mask = (1<<31);
	for(i=0; i < 32; i++, mask=(mask>>1)) {
		if ((x & mask) != 0) {
			break;
		}
	}
	return i;
#endif
}

static uint32_t
quick_dirty_insecure_random(uint32_t ceiling)
{
	static uint32_t state = SOOT_CONFIG_DEBUG_STREAM_CORRUPT_SEED;
	uint32_t ret;
	uint8_t shift = clz32(ceiling-1);

	if (ceiling == 0) {
		return 0;
	}

	do {
		ret = state = state * 1664525 + 1013904223;
		ret >>= shift;
	} while (ret >= ceiling);
	return ret;
}
#endif // #if SOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_FACTOR || SOOT_CONFIG_DEBUG_TX_STREAM_CORRUPT_FACTOR

#if DEBUG
NOT_USED_ATTR static const char*
soot_packet_type_to_string(soot_packet_type_t x)
{
	const char* ret = NULL;
	switch (x) {
	case SOOT_PACKET_TYPE_RESET: ret = "RESET"; break;
	case SOOT_PACKET_TYPE_RSTACK: ret = "RSTACK"; break;
	case SOOT_PACKET_TYPE_ACK: ret = "ACK"; break;
	case SOOT_PACKET_TYPE_NACK: ret = "NACK"; break;
	}
	return ret;
}

NOT_USED_ATTR static const char*
soot_tx_buffer_state_to_string(soot_tx_buffer_state_t x)
{
	const char* ret = NULL;
	switch (x) {
	case SOOT_TX_BUFFER_STATE_READY:       ret = "READY"; break;
	case SOOT_TX_BUFFER_STATE_FLUSHING:    ret = "FLUSH"; break;
	case SOOT_TX_BUFFER_STATE_WAITING:     ret = "WAITING"; break;
	case SOOT_TX_BUFFER_STATE_ACKNOWLEGED: ret = "ACKED"; break;
	}
	return ret;
}

NOT_USED_ATTR static void
soot_tx_buffer_DEBUG_DUMP(soot_tx_buffer_t *buffer, const char* desc, bool is_hot)
{
	DEBUG_PRINTF("soot:tx_buff_dump: %s%c TYPE:%s OFC:%d AFC:%d State:%s NextCMS:%d BytesSent:%d Len:%d PLen:%d",
		desc,
		is_hot?'*':' ',
		soot_packet_type_to_string(buffer->mType),
		buffer->mOFC,
		buffer->mAFC,
		soot_tx_buffer_state_to_string(buffer->mState),
		buffer->mNextAttempt?buffer->mNextAttempt-soot_hook_get_ms():-1,
		buffer->mBytesSent,
		buffer->mLen,
		buffer->mData.mFrame.mPayloadLen
	);
	DEBUG_DUMP(buffer, desc, buffer->mData.mRaw, buffer->mLen);
}
#endif // DEBUG

// **** LCOV_EXCL_STOP ****

static uint16_t
soot_crc16(uint16_t crc_state, uint8_t byte)
{
	crc_state = (uint16_t)((crc_state >> 8) | (crc_state << 8));
	crc_state ^= byte;
	crc_state ^= ((crc_state & 0xff) >> 4);
	crc_state ^= (crc_state << 12);
	crc_state ^= ((crc_state & 0xff) << 5);

	return crc_state;
}

static bool
soot_byte_needs_escape(uint8_t byte)
{
	switch(byte) {
	case SOOT_BYTE_SPECIAL:
	case SOOT_BYTE_ESC:
	case SOOT_BYTE_FLAG:
	case SOOT_BYTE_XOFF:
	case SOOT_BYTE_XON:
		return true;

	default:
		return false;
	}
}


// ----------------------------------------------------------------------------
// MARK: -
// MARK: Hooks and Callbacks

#if !SOOT_EMBEDDED
void
soot_set_callbacks(
	soot_instance_t* self,
	soot_tx_callback_func tx_callback,
	soot_read_callback_func read_callback,
	void* context
) {
	self->mCallbackTXFunc = tx_callback;
	self->mCallbackReadFunc = read_callback;
	self->mCallbackContext = context;
}

static ssize_t
soot_hook_tx_data(soot_instance_t* self, const void *buf, size_t nbyte)
{
	return (*self->mCallbackTXFunc)(self->mCallbackContext, buf, nbyte);
}

static ssize_t
soot_hook_read_data(soot_instance_t* self, const void *buf, size_t nbyte)
{
	return (*self->mCallbackReadFunc)(self->mCallbackContext, buf, nbyte);
}
#endif


// ----------------------------------------------------------------------------
// MARK: -
// MARK: Low-Level TX Buffer Management

static void
soot_tx_buffer_set_ofc(soot_tx_buffer_t *buffer, uint8_t ofc)
{
	assert((ofc & ~SOOT_FRAME_COUNTER_MASK) == 0);

	buffer->mData.mFrame.mCtrl &= ~SOOT_CNTRL_OUTGOING_FRAME_COUNTER_MASK;
	buffer->mData.mFrame.mCtrl |= (ofc << 3);

	// This will get updated later by `soot_tx_buffer_prep_for_flush()`
	// if necessary.
	buffer->mData.mFrame.mHeaderEscape &= ~SOOT_HIGH_OCTET_NEEDS_ESCAPE_FLAG;

	buffer->mOFC = ofc;
}

static void
soot_tx_buffer_set_afc(soot_tx_buffer_t *buffer, uint8_t afc)
{
	assert((afc & ~SOOT_FRAME_COUNTER_MASK) == 0);

	buffer->mData.mFrame.mCtrl &= ~SOOT_CNTRL_ACK_NACK_FRAME_COUNTER_MASK;
	buffer->mData.mFrame.mCtrl |= (afc << 0);

	buffer->mAFC = afc;
}

static void
soot_tx_buffer_set_type(soot_tx_buffer_t *buffer, soot_packet_type_t type)
{
	buffer->mData.mFrame.mCtrl &= ~SOOT_CNTRL_TYPE_MASK;
	buffer->mData.mFrame.mCtrl |= (type << 6);

	buffer->mType = type;
}

static void
soot_tx_buffer_reset(soot_tx_buffer_t *buffer)
{
	buffer->mData.mFrame.mAshFlag = SOOT_BYTE_FLAG;
	buffer->mState = SOOT_TX_BUFFER_STATE_READY;
	buffer->mData.mFrame.mPayloadLen = 0;
	buffer->mData.mFrame.mHeaderEscape = 0;
	buffer->mNextAttempt = 0;
	buffer->mRetries = 0;
	buffer->mBytesSent = 0;
	buffer->mLen = 0;

	soot_tx_buffer_set_type(buffer, SOOT_PACKET_TYPE_ACK);
}

static void
soot_tx_buffer_recalc_crc(soot_tx_buffer_t *buffer)
{
	uint8_t i;
	uint16_t crc = 0;

	assert(buffer->mLen >= SOOT_CRC_SIZE);

	// Calculate the 16-bit CRC
	for (i = 0; i < buffer->mLen - SOOT_CRC_SIZE; i++) {
#if SOOT_CONFIG_DEBUG_TX_STREAM_CORRUPT_FACTOR
		if (quick_dirty_insecure_random(SOOT_CONFIG_DEBUG_TX_STREAM_CORRUPT_WINDOW) < SOOT_CONFIG_DEBUG_TX_STREAM_CORRUPT_FACTOR) {
			crc ^= 0xDEAD;
		}
#endif
		crc = soot_crc16(crc, buffer->mData.mRaw[i]);
	}

	// Encode the CRC to avoid possibly escaped representation
	buffer->mData.mRaw[buffer->mLen - SOOT_CRC_SIZE + 0] = ((crc >> 8) & ~SOOT_MAGIC_ESCAPE_BIT);
	buffer->mData.mRaw[buffer->mLen - SOOT_CRC_SIZE + 1] = ((crc >> 0) & ~SOOT_MAGIC_ESCAPE_BIT);
	buffer->mData.mRaw[buffer->mLen - SOOT_CRC_SIZE + 2] = (uint8_t)((((crc >> 8) & SOOT_MAGIC_ESCAPE_BIT) << 3) | (((crc >> 0) & SOOT_MAGIC_ESCAPE_BIT) << 2));
}

static void
soot_tx_buffer_prep_for_flush(soot_tx_buffer_t *buffer)
{
	// Note: The OFC and AFC should be already set by the time this is called.

	if (buffer->mState == SOOT_TX_BUFFER_STATE_READY) {
		buffer->mLen = buffer->mData.mFrame.mPayloadLen + SOOT_HEADER_SIZE + SOOT_CRC_SIZE;

		assert(buffer->mLen <= SOOT_MAX_PACKET_SIZE);

		// Transform the payload length byte, if needed.
		if (soot_byte_needs_escape(buffer->mData.mFrame.mPayloadLen)) {
			buffer->mData.mFrame.mHeaderEscape |= SOOT_LOW_OCTET_NEEDS_ESCAPE_FLAG;
			buffer->mData.mFrame.mPayloadLen      ^= SOOT_ESCAPE_XFORM;
		} else {
			buffer->mData.mFrame.mHeaderEscape &= ~SOOT_LOW_OCTET_NEEDS_ESCAPE_FLAG;
		}
	}

	if (buffer->mData.mFrame.mHeaderEscape & SOOT_HIGH_OCTET_NEEDS_ESCAPE_FLAG) {
		buffer->mData.mFrame.mCtrl ^= SOOT_ESCAPE_XFORM;
	}

	if (soot_byte_needs_escape(buffer->mData.mFrame.mCtrl)) {
		buffer->mData.mFrame.mHeaderEscape |= SOOT_HIGH_OCTET_NEEDS_ESCAPE_FLAG;
		buffer->mData.mFrame.mCtrl ^= SOOT_ESCAPE_XFORM;
	} else {
		buffer->mData.mFrame.mHeaderEscape &= ~SOOT_HIGH_OCTET_NEEDS_ESCAPE_FLAG;
	}

	buffer->mBytesSent = 0;
	buffer->mState = SOOT_TX_BUFFER_STATE_FLUSHING;
	buffer->mNextAttempt = 0;

	assert((buffer->mType != SOOT_PACKET_TYPE_RESET) || (buffer->mOFC == 1));
}


// ----------------------------------------------------------------------------
// MARK: -
// MARK: TX Buffer Retrieval Logic

static soot_tx_buffer_t*
soot_current_tx_buffer(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	return self->mHotBuffer;
}

static soot_tx_buffer_t*
soot_prev_tx_buffer(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	return &self->mTXBuffers[self->mHotBuffer == self->mTXBuffers];
}

//! Returns the next writable buffer.
static soot_tx_buffer_t*
soot_next_writable_tx_buffer(soot_instance_t* self)
{
	soot_tx_buffer_t *buffer = soot_current_tx_buffer(self);

	if (SOOT_TX_BUFFER_STATE_READY == buffer->mState) {
		if (buffer->mData.mFrame.mPayloadLen < SOOT_MAX_PAYLOAD_SIZE-1) {
			return buffer;
		}
		buffer = soot_prev_tx_buffer(self);
		if (SOOT_TX_BUFFER_STATE_ACKNOWLEGED == buffer->mState) {
			return buffer;
		}
	}

	buffer = soot_prev_tx_buffer(self);

	if (SOOT_TX_BUFFER_STATE_READY == buffer->mState) {
		if (buffer->mData.mFrame.mPayloadLen < SOOT_MAX_PAYLOAD_SIZE-1) {
			return buffer;
		}
	}

	if (SOOT_TX_BUFFER_STATE_ACKNOWLEGED == buffer->mState) {
		return buffer;
	}

	return NULL;
}

//! Returns a buffer that needs to be transmitted.
static soot_tx_buffer_t*
soot_next_transmitable_tx_buffer(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	int i;

	for (i = 0; i < 2; i++) {
		soot_tx_buffer_t * const buffer = (i == 0)
			? soot_prev_tx_buffer(self)
			: soot_current_tx_buffer(self);

		if (SOOT_TX_BUFFER_STATE_FLUSHING == buffer->mState) {
			return buffer;
		}
	}

	for (i = 0; i < 2; i++) {
		soot_tx_buffer_t * const buffer = (i == 0)
			? soot_prev_tx_buffer(self)
			: soot_current_tx_buffer(self);

		if ((SOOT_TX_BUFFER_STATE_WAITING == buffer->mState)
		 && (buffer->mNextAttempt
			 ? CMS_SINCE(buffer->mNextAttempt) >= 0
			 : false )
		) {
			return buffer;
		}
	}

	for (i = 0; i < 2; i++) {
		soot_tx_buffer_t * const buffer = (i == 0)
			? soot_prev_tx_buffer(self)
			: soot_current_tx_buffer(self);

		if ((SOOT_TX_BUFFER_STATE_READY == buffer->mState)
		 && (buffer->mData.mFrame.mPayloadLen != 0)
		) {
			return buffer;
		}
	}

	return NULL;
}


// ----------------------------------------------------------------------------
// MARK: -
// MARK: High-Level TX Buffer Management

//! Transitions TX Buffer from ACKED to READY and updates OFC
static void
soot_outbound_frame_recycle(soot_instance_t* self, soot_tx_buffer_t *buffer)
{
	SOOT_EMBEDDED_SELF_HOOK;

	check(buffer->mState == SOOT_TX_BUFFER_STATE_ACKNOWLEGED);
	check(self->mNextOutboundFrame != buffer->mOFC);
	check(buffer != self->mHotBuffer);

	soot_tx_buffer_reset(buffer);
	soot_tx_buffer_set_ofc(buffer, self->mNextOutboundFrame);
	self->mNextOutboundFrame = SOOT_FRAME_INC(self->mNextOutboundFrame);
}

//! Transitions TX Buffer from WAITING to ACKED
static void
soot_outbound_frame_did_ack(soot_instance_t* self, soot_tx_buffer_t *buffer)
{
	SOOT_EMBEDDED_SELF_HOOK;

	DEBUG_PRINTF("soot:%p:was_ackd:  TYPE:%s OFC:%d AFC:%d PacketLen:%d bytesSent:%d Retry: %d",
		self,
		soot_packet_type_to_string(buffer->mType),
		buffer->mOFC,
		buffer->mAFC,
		buffer->mLen,
		buffer->mBytesSent,
		buffer->mRetries
	);

	check((buffer->mState != SOOT_TX_BUFFER_STATE_ACKNOWLEGED) || (buffer->mType == SOOT_PACKET_TYPE_RSTACK));
	check(buffer->mState != SOOT_TX_BUFFER_STATE_READY);

	assert(SOOT_FRAME_INC(self->mLastAckedOutboundFrame) == buffer->mOFC);

	if (SOOT_FRAME_INC(self->mLastAckedOutboundFrame) == buffer->mOFC) {
		self->mLastAckedOutboundFrame = buffer->mOFC;
	}

	if ((buffer->mState == SOOT_TX_BUFFER_STATE_WAITING)
	 || (buffer->mState == SOOT_TX_BUFFER_STATE_FLUSHING)
	) {
		soot_tx_buffer_set_type(buffer, SOOT_PACKET_TYPE_ACK);
		buffer->mState = SOOT_TX_BUFFER_STATE_ACKNOWLEGED;
		buffer->mLen = SOOT_HEADER_SIZE+SOOT_CRC_SIZE;
		buffer->mBytesSent = 0;
		buffer->mNextAttempt = 0;
		buffer->mRetries = 0;
		buffer->mData.mFrame.mPayloadLen = 0;
		buffer->mData.mFrame.mHeaderEscape &= ~SOOT_LOW_OCTET_NEEDS_ESCAPE_FLAG;
	}

	if ((buffer != self->mHotBuffer)
	 && (self->mHotBuffer->mState == SOOT_TX_BUFFER_STATE_ACKNOWLEGED)
	) {
		soot_outbound_frame_recycle(self, buffer);
	}
}

//! Transitions TX Buffer from anything to FLUSH
static void
soot_outbound_frame_flush(soot_instance_t* self, soot_tx_buffer_t *buffer)
{
	SOOT_EMBEDDED_SELF_HOOK;

	check((buffer->mState != SOOT_TX_BUFFER_STATE_ACKNOWLEGED) || (buffer->mData.mFrame.mPayloadLen == 0));

	if ((self->mSootState != SOOT_STATE_RUNNING)
	 && (buffer->mType == SOOT_PACKET_TYPE_RESET)
	 && (self->mNextRXOFC == 1)
	) {
		soot_tx_buffer_set_afc(buffer, 0);
	} else {
		soot_tx_buffer_set_afc(buffer, SOOT_FRAME_DEC(self->mNextRXOFC));
	}

	soot_tx_buffer_prep_for_flush(buffer);
}

//! Transitions TX Buffer from FLUSH to WAITING or ACKED
static void
soot_outbound_frame_did_flush(soot_instance_t* self, soot_tx_buffer_t *buffer)
{
	SOOT_EMBEDDED_SELF_HOOK;

	check(buffer->mState == SOOT_TX_BUFFER_STATE_FLUSHING);

	if ((self->mSootState == SOOT_STATE_RESET)
	 && (buffer->mType == SOOT_PACKET_TYPE_RSTACK)
	) {
		DEBUG_PRINTF("soot:%p: Sent RSTACK, but needs to send RESET", self);
		soot_tx_buffer_set_type(buffer, SOOT_PACKET_TYPE_RESET);
		buffer->mBytesSent = 0;
		buffer->mRetries = 0;
		soot_tx_buffer_prep_for_flush(buffer);
        // Send it 50ms from now
		buffer->mNextAttempt = soot_hook_get_ms() + SOOT_ACK_TIMEOUT_MS/10;
		buffer->mState = SOOT_TX_BUFFER_STATE_WAITING;

	} else if ((buffer->mLen == SOOT_HEADER_SIZE+SOOT_CRC_SIZE)
		&& (buffer->mType != SOOT_PACKET_TYPE_RESET)
	) {
		buffer->mState = SOOT_TX_BUFFER_STATE_ACKNOWLEGED;
		buffer->mRetries = 0;
		buffer->mBytesSent = 0;
		buffer->mNextAttempt = 0;
	} else {
		// Set the timeout.
		buffer->mState = SOOT_TX_BUFFER_STATE_WAITING;
		buffer->mNextAttempt = soot_hook_get_ms() + SOOT_ACK_TIMEOUT_MS;
		buffer->mBytesSent = 0;
		buffer->mRetries++;
	}
}


// ----------------------------------------------------------------------------
// MARK: -
// MARK: Outbound Data Processing

static void
soot_signal_ack(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	soot_tx_buffer_t *buffer = soot_current_tx_buffer(self);

	if ((buffer->mState == SOOT_TX_BUFFER_STATE_WAITING)
	 && (soot_prev_tx_buffer(self)->mState == SOOT_TX_BUFFER_STATE_ACKNOWLEGED)
	) {
		soot_tx_buffer_t *prev_buffer = soot_prev_tx_buffer(self);
		soot_tx_buffer_set_ofc(prev_buffer, buffer->mOFC);
		buffer = prev_buffer;
	}

	if ((buffer->mState != SOOT_TX_BUFFER_STATE_FLUSHING)
	 || (buffer->mBytesSent == 0)
	) {
		soot_tx_buffer_set_type(buffer, SOOT_PACKET_TYPE_ACK);
		soot_outbound_frame_flush(self, buffer);
	} else {
		DEBUG_PRINTF("soot:%p:signal_ack: No free buffers to send ACK...?", self); // LCOV_EXCL_LINE
	}
}

static void
soot_signal_nack(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	soot_tx_buffer_t *buffer = soot_current_tx_buffer(self);

	if ((buffer->mState == SOOT_TX_BUFFER_STATE_WAITING)
	 && (soot_prev_tx_buffer(self)->mState == SOOT_TX_BUFFER_STATE_ACKNOWLEGED)
	) {
		soot_tx_buffer_t *prev_buffer = soot_prev_tx_buffer(self);
		soot_tx_buffer_set_ofc(prev_buffer, buffer->mOFC);
		buffer = prev_buffer;
	}

	if ((buffer->mState != SOOT_TX_BUFFER_STATE_FLUSHING)
	 || (buffer->mBytesSent == 0)
	) {
		soot_tx_buffer_set_type(buffer, SOOT_PACKET_TYPE_NACK);
	} else {
		DEBUG_PRINTF("soot:%p:signal_nack: No free buffers to send NACK...?", self);  // LCOV_EXCL_LINE
	}

	if (buffer->mType == SOOT_PACKET_TYPE_NACK) {
		if (buffer->mState != SOOT_TX_BUFFER_STATE_FLUSHING) {
			soot_outbound_frame_flush(self, buffer);
		}
	}
}

bool
soot_wants_to_tx_data(const soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;

	return soot_next_transmitable_tx_buffer((soot_instance_t*)self) != NULL;
}

void
soot_process_tx_data(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	soot_tx_buffer_t *buffer;
	ssize_t bytes_sent;

	while ((buffer = soot_next_transmitable_tx_buffer(self))) {

		if (buffer->mState != SOOT_TX_BUFFER_STATE_FLUSHING) {
			soot_outbound_frame_flush(self, buffer);
		}

		if (buffer->mBytesSent == 0) {
			soot_tx_buffer_recalc_crc(buffer);
		}

		if (soot_is_connected(self)) {
			require_action(
				buffer->mRetries < SOOT_CONFIG_MAX_RETRIES,
				bail,
				soot_reset(self)
			);
		}

		DEBUG_PRINTF("soot:%p:sending:   TYPE:%s OFC:%d AFC:%d PacketLen:%d bytesSent:%d Retry: %d",
			self,
			soot_packet_type_to_string(buffer->mType),
			buffer->mOFC,
			buffer->mAFC,
			buffer->mLen,
			buffer->mBytesSent,
			buffer->mRetries
		);

		// Checks to ensure that the packet we are about to send is sane.
		// These get compiled out unless we are built as DEBUG.
		assert((buffer->mAFC != 0) || (buffer->mType==SOOT_PACKET_TYPE_RESET));
		assert(!soot_byte_needs_escape(buffer->mData.mFrame.mCtrl));
		assert(!soot_byte_needs_escape(buffer->mData.mFrame.mPayloadLen));

		buffer->mNextAttempt = 0;

		bytes_sent = soot_hook_tx_data(
			self,
			buffer->mData.mRaw + buffer->mBytesSent,
			buffer->mLen - buffer->mBytesSent
		);

		if (bytes_sent == 0) {
			break;
		}

		require(bytes_sent >= 0, bail);

		DEBUG_DUMP(
			self,
			"bytes_sent",
			buffer->mData.mRaw + buffer->mBytesSent,
			bytes_sent
		);

		buffer->mBytesSent += bytes_sent;

		if (buffer->mBytesSent == buffer->mLen) {
			soot_outbound_frame_did_flush(self, buffer);
		} else {
			break;
		}

	}

bail:

	return;
}


// ----------------------------------------------------------------------------
// MARK: -
// MARK: General Public Methods

static void
soot_init(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;

	memset(self, 0, sizeof(*self));
	soot_reset(self);
	self->mDidReset = 0;
}

static void
soot_finalize(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	DEBUG_PRINTF("soot:%p: FINALIZE", self);
	// We don't actually malloc anything, so there is nothing to do here.
	// We keep this function around as a hook in case it is needed later.
}

soot_instance_t*
soot_create(void)
{
	soot_instance_t* self;

#if SOOT_EMBEDDED
	self = &gSootGlobalInstance;
#else
	self = malloc(sizeof(soot_instance_t));
#endif

	if (self != NULL) {
		soot_init(self);
	}

	return self;

}

void
soot_release(soot_instance_t* self)
{
	soot_finalize(self);
#if !SOOT_EMBEDDED
	free(self);
#endif
}

void
soot_reset(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	DEBUG_PRINTF("soot:%p: **** RESET ****", self);

	if (SOOT_STATE_RUNNING == self->mSootState) {
		self->mDidReset = 1;
	}

	self->mNextRXOFC = 1;
	self->mLastAckedOutboundFrame = 0;
	self->mNextOutboundFrame = 3;

#if SOOT_CONFIG_HANDLE_OUT_OF_ORDER
	self->mOOORXLen = 0;
#endif

	soot_tx_buffer_reset(&self->mTXBuffers[0]);
	soot_tx_buffer_reset(&self->mTXBuffers[1]);

	soot_tx_buffer_set_ofc(&self->mTXBuffers[0], 2);
	soot_tx_buffer_set_ofc(&self->mTXBuffers[1], 1);

	self->mHotBuffer = &self->mTXBuffers[1];

	self->mSootState = SOOT_STATE_RESET;

	DP_INIT(&self->mRXDataDP);

	DEBUG_PRINTF("soot:%p: SENDING RESET", self);

	soot_tx_buffer_set_type(self->mHotBuffer, SOOT_PACKET_TYPE_RESET);
	soot_outbound_frame_flush(self, self->mHotBuffer);
}

bool
soot_is_connected(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	return self->mSootState == SOOT_STATE_RUNNING;
}

bool
soot_did_reset(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	if (self->mDidReset) {
		self->mDidReset = 0;
		return true;
	}
	return false;
}

soot_status_t
soot_get_last_status(const soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	if (self->mDidReset) {
		return SOOT_STATUS_RESET;
	}
	return SOOT_STATUS_OK;
}

cms_t
soot_get_timeout(const soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	cms_t ret = CMS_DISTANT_FUTURE;
	int i;

	for (i = 0; i < 2; i++) {
		if ((self->mTXBuffers[i].mState == SOOT_TX_BUFFER_STATE_WAITING)
		 && (self->mTXBuffers[i].mNextAttempt != 0)
		) {
			cms_t tmp = self->mTXBuffers[i].mNextAttempt - soot_hook_get_ms();
			if (tmp < ret) {
				ret = tmp;
			}
		}
	}

	// Prevent negative return values.
	if (ret < 0) {
		ret = 0;
	}

	return ret;
}


// ----------------------------------------------------------------------------
// MARK: -
// MARK: Inbound Data Processing

//! Called when we receive a RESET frame.
static void
soot_handle_rx_reset(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	soot_tx_buffer_t *buffer;

	DEBUG_PRINTF("soot:%p: RECEIVED RESET", self);

	if ((self->mRXAFC == 0) && soot_is_connected(self)) {
		self->mDidReset = true;
#if SOOT_CONFIG_HANDLE_OUT_OF_ORDER
		self->mOOORXLen = 0;
#endif
		self->mNextRXOFC = 1;
		self->mLastAckedOutboundFrame = 0;
		self->mNextOutboundFrame = 3;

		soot_tx_buffer_reset(&self->mTXBuffers[0]);
		soot_tx_buffer_reset(&self->mTXBuffers[1]);

		soot_tx_buffer_set_ofc(&self->mTXBuffers[0], 2);
		soot_tx_buffer_set_ofc(&self->mTXBuffers[1], 1);

		self->mHotBuffer = &self->mTXBuffers[1];
	}

	check(self->mRXOFC == 1);

	self->mNextRXOFC = SOOT_FRAME_INC(self->mRXOFC);

	buffer = soot_current_tx_buffer(self);

	assert(buffer != NULL);

	DEBUG_PRINTF("soot:%p: SENDING RSTACK", self);

	soot_tx_buffer_reset(buffer);
	soot_tx_buffer_set_type(buffer, SOOT_PACKET_TYPE_RSTACK);
	soot_outbound_frame_flush(self, buffer);

	DP_INIT(&self->mRXDataDP);
}

//! Handles any TX buffers that have been acked by the received frame
static void
soot_handle_rx_packet_afc(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	soot_tx_buffer_t *buffer = NULL;

	if ((self->mLastAckedOutboundFrame != 0)
	 && (SOOT_FRAME_INC(SOOT_FRAME_INC(self->mLastAckedOutboundFrame)) == self->mRXAFC)
	) {
		soot_outbound_frame_did_ack(self, soot_prev_tx_buffer(self));
		soot_outbound_frame_recycle(self, soot_prev_tx_buffer(self));
		soot_outbound_frame_did_ack(self, soot_current_tx_buffer(self));

	} else if (SOOT_FRAME_INC(self->mLastAckedOutboundFrame) == self->mRXAFC) {
		buffer = soot_current_tx_buffer(self);

		if ((buffer->mOFC == self->mRXAFC)
		 && ((buffer->mState == SOOT_TX_BUFFER_STATE_WAITING) || (buffer->mBytesSent == 0))
		) {
			soot_outbound_frame_did_ack(self, buffer);
		}

		buffer = soot_prev_tx_buffer(self);

		if (buffer->mState == SOOT_TX_BUFFER_STATE_ACKNOWLEGED) {
			soot_outbound_frame_recycle(self, buffer);
		}

		if (self->mTXBuffers[0].mOFC != self->mTXBuffers[1].mOFC) {
			// We are not re-using the previous buffer for an ack or a nack.

			if (buffer->mOFC == self->mRXAFC) {
				check((buffer->mState == SOOT_TX_BUFFER_STATE_WAITING) || (buffer->mBytesSent == 0));
				soot_outbound_frame_did_ack(self, buffer);
			}
		}

	} else if (self->mLastAckedOutboundFrame == self->mRXAFC) {
		// Do nothing.

	} else if (SOOT_FRAME_DEC(self->mLastAckedOutboundFrame) == self->mRXAFC) {
		// We don't worry about this case.

	} else {
		// We are out of sync. Reset.
		ERROR_PRINTF("soot:%p: **** OUT OF SYNC ****", self);
		soot_reset(self);
		return;
	}
}

static void
soot_handle_rx_packet(soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	soot_tx_buffer_t *buffer = NULL;

	DEBUG_PRINTF("soot:%p:rx_packet: TYPE:%s AFC:%d OFC:%d DecodedPayloadLen:%d RXCRC:0x%04x",
		self,
		soot_packet_type_to_string(self->mRXType),
		self->mRXAFC,
		self->mRXOFC,
		self->mRXLen,
		self->mRXCRC
	);


	if (self->mSootState == SOOT_STATE_RUNNING) {
		switch (self->mRXType) {
		case SOOT_PACKET_TYPE_RSTACK:
		case SOOT_PACKET_TYPE_ACK:
			break;

		case SOOT_PACKET_TYPE_NACK:
			// Go ahead and retransmit if necessary
			buffer = soot_prev_tx_buffer(self);

			if ((SOOT_FRAME_INC(self->mRXAFC) == buffer->mOFC)
			 && (buffer->mState == SOOT_TX_BUFFER_STATE_WAITING)
			) {
				soot_outbound_frame_flush(self, buffer);
				buffer = soot_current_tx_buffer(self);
				if (buffer->mState == SOOT_TX_BUFFER_STATE_WAITING) {
					DEBUG_PRINTF("soot:%p:rx_packet: Will retransmit",self);
					soot_outbound_frame_flush(self, buffer);
				}
			} else {
				buffer = soot_current_tx_buffer(self);

				if ((SOOT_FRAME_INC(self->mRXAFC) == buffer->mOFC)
				 && (buffer->mState == SOOT_TX_BUFFER_STATE_WAITING)
				) {
					DEBUG_PRINTF("soot:%p:rx_packet: Will retransmit",self);
					soot_outbound_frame_flush(self, buffer);
				}
			}
			break;

		case SOOT_PACKET_TYPE_RESET:
			soot_handle_rx_reset(self);
			return;
			break;
		}

		// Handle any TX buffers that have been acked by this frame.
		soot_handle_rx_packet_afc(self);

		if (self->mDidReset) {
			return;
		}

	} else {
		// In this case we aren't running yet.
		switch (self->mRXType) {

		case SOOT_PACKET_TYPE_ACK:
			buffer = &self->mTXBuffers[1];
#if SOOT_CONFIG_MISSING_RSTACK_WORKAROUND
			if ((self->mRXAFC == 1)
			 && (buffer->mType == SOOT_PACKET_TYPE_RESET)
			) {
				// Work around for a bug in SiLabs' ASHv3 implementation.
				WARNING_PRINTF("soot:%p: **** SOOT_STATE_RUNNING **** (work-around)", self);
				self->mSootState = SOOT_STATE_RUNNING;
				self->mNextRXOFC = self->mRXOFC;
				if (buffer->mType == SOOT_PACKET_TYPE_RESET) {
					soot_outbound_frame_did_ack(self, buffer);
				}
				break;
			}
#endif
		case SOOT_PACKET_TYPE_NACK:
			if ((self->mRXOFC > 2) && (self->mNextRXOFC > 1)) {
				soot_reset(self);
				return;
			}
			// Drop ACK and NACK packets if we aren't running yet.
			return;
			break;

		case SOOT_PACKET_TYPE_RESET:
			soot_handle_rx_reset(self);
			break;

		case SOOT_PACKET_TYPE_RSTACK:
			DEBUG_PRINTF("soot:%p: **** SOOT_STATE_RUNNING ****", self);
			check(self->mRXAFC == 1);
			self->mSootState = SOOT_STATE_RUNNING;
			self->mNextRXOFC = SOOT_FRAME_INC(self->mRXOFC);
			buffer = &self->mTXBuffers[1];
			if ((buffer->mType == SOOT_PACKET_TYPE_RESET)
			 || (buffer->mType == SOOT_PACKET_TYPE_RSTACK)
			) {
				soot_outbound_frame_did_ack(self, buffer);
			}
			break;
		}
	}




	if (self->mRXOFC == self->mNextRXOFC) {
		if (self->mRXLen) {
			ssize_t bytes_read;

			bytes_read = soot_hook_read_data(self, self->mRXDataBuffer, self->mRXLen);

			// We don't support partial data being pushed out
			assert((bytes_read == self->mRXLen) || (bytes_read <= 0));

			if (bytes_read == self->mRXLen) {
				self->mNextRXOFC = SOOT_FRAME_INC(self->mNextRXOFC);

#if SOOT_CONFIG_HANDLE_OUT_OF_ORDER
				if (self->mOOORXLen != 0) {
					bytes_read = soot_hook_read_data(
						self,
						self->mOOORXDataBuffer,
						self->mOOORXLen
					);

					assert((bytes_read == self->mOOORXLen) || (bytes_read <= 0));

					if (bytes_read == self->mOOORXLen) {
						WARNING_PRINTF("soot:%p: Handled RXOFC %d out-of-order", self, self->mNextRXOFC);
						self->mNextRXOFC = SOOT_FRAME_INC(self->mNextRXOFC);
					}

					self->mOOORXLen = 0;
				}
#endif
				soot_signal_ack(self);
			} else {
				WARNING_PRINTF("soot:%p: Throttling", self);
			}
		} else {
			// This was a new OFC, but it didn't contain any data, which
			// normally shouldn't happen, but can occur when used with other
			// ASHv3-compatible implementations.
		}

	} else if ((self->mRXOFC == SOOT_FRAME_DEC(self->mNextRXOFC))
			|| (self->mRXOFC == SOOT_FRAME_DEC(SOOT_FRAME_DEC(self->mNextRXOFC)))
	) {
		// We've already handled this packet, so all we need to do is ack.
		if (self->mRXLen) {
			soot_signal_ack(self);
		}

	} else if (self->mRXOFC == SOOT_FRAME_INC(self->mNextRXOFC)) {
		// Seems like we missed a packet...? Send a NACK so that we
		// can coax the other side to send it to us faster.
		soot_signal_nack(self);

#if SOOT_CONFIG_HANDLE_OUT_OF_ORDER
		if (self->mRXLen && !self->mOOORXLen) {
			self->mOOORXLen = self->mRXLen;
			memcpy(self->mOOORXDataBuffer, self->mRXDataBuffer, sizeof(self->mRXDataBuffer));
			WARNING_PRINTF("soot:%p: Will handle RXOFC %d out-of-order", self, self->mRXOFC);
		}
#endif
	} else {
		// Any other value for mRXOFC indicates that the remote side is
		// misbehaving. We reset so that we can get back in sync
		// as quickly as possible.
		ERROR_PRINTF("soot:%p: Remote device appears to be misbehaving.", self);
		soot_reset(self);
	}

}

static int
soot_rx_data_parser(soot_instance_t* self, const uint8_t* bytes, size_t len)
{
	SOOT_EMBEDDED_SELF_HOOK;
	uint8_t byte, i, j;

	assert(bytes != NULL);

	DP_BEGIN(&self->mRXDataDP);

	// Read the start-of-frame byte
	DP_READ_BYTE(&self->mRXDataDP, &byte);
	self->mRXCalcCRC = soot_crc16(0, byte);

	require(byte == SOOT_BYTE_FLAG, on_error);

	// "header escape" byte.
	DP_READ_BYTE(&self->mRXDataDP, &byte);
	self->mRXCalcCRC = soot_crc16(self->mRXCalcCRC, byte);
	self->mRXHdrEsc = byte;

	// Read the control byte
	DP_READ_BYTE(&self->mRXDataDP, &byte);
	self->mRXCalcCRC = soot_crc16(self->mRXCalcCRC, byte);
	if (self->mRXHdrEsc & SOOT_HIGH_OCTET_NEEDS_ESCAPE_FLAG) {
		byte ^= SOOT_ESCAPE_XFORM;
		check(soot_byte_needs_escape(byte));
	} else {
		check(!soot_byte_needs_escape(byte));
	}
	self->mRXType = ((byte & SOOT_CNTRL_TYPE_MASK) >> 6);
	self->mRXOFC  = ((byte & SOOT_CNTRL_OUTGOING_FRAME_COUNTER_MASK) >> 3);
	self->mRXAFC  = ((byte & SOOT_CNTRL_ACK_NACK_FRAME_COUNTER_MASK)>> 0);

	// Read the length byte
	DP_READ_BYTE(&self->mRXDataDP, &byte);
	self->mRXCalcCRC = soot_crc16(self->mRXCalcCRC, byte);
	if (self->mRXHdrEsc & SOOT_LOW_OCTET_NEEDS_ESCAPE_FLAG) {
		byte ^= SOOT_ESCAPE_XFORM;
		check(soot_byte_needs_escape(byte));
	} else {
		check(!soot_byte_needs_escape(byte));
	}
	self->mRXLen = byte;

	require(self->mRXLen <= SOOT_MAX_PAYLOAD_SIZE, on_error);

	// Read the payload (escaped)
	DP_READ_DATA(&self->mRXDataDP, self->mRXDataBuffer, self->mRXLen);

	// Feed the payload to the CRC calculation. We also go ahead and
	// unescape the data here, too.
	for (i = 0, j = 0; j < self->mRXLen; i++, j++) {
		self->mRXCalcCRC = soot_crc16(self->mRXCalcCRC, self->mRXDataBuffer[i]);
		if (self->mRXDataBuffer[i] == SOOT_BYTE_ESC) {
			i++;
			self->mRXLen--;
			self->mRXCalcCRC = soot_crc16(self->mRXCalcCRC, self->mRXDataBuffer[i]);
			self->mRXDataBuffer[j] = (self->mRXDataBuffer[i] ^ SOOT_ESCAPE_XFORM);
			check(soot_byte_needs_escape(self->mRXDataBuffer[j]));
		} else {
			self->mRXDataBuffer[j] = self->mRXDataBuffer[i];
		}
	}

	// Read the first byte of the CRC
	DP_READ_BYTE(&self->mRXDataDP, &byte);
	self->mRXCRC = (uint16_t)(byte << 8);

	// Read the second byte of the CRC
	DP_READ_BYTE(&self->mRXDataDP, &byte);
	self->mRXCRC |= byte;

	// Read the final byte of the CRC
	DP_READ_BYTE(&self->mRXDataDP, &byte);

	// Perform escaping magic
	if (byte & SOOT_HIGH_OCTET_NEEDS_ESCAPE_FLAG) {
		self->mRXCRC |= (SOOT_MAGIC_ESCAPE_BIT << 8);
	}
	if (byte & SOOT_LOW_OCTET_NEEDS_ESCAPE_FLAG) {
		self->mRXCRC |= SOOT_MAGIC_ESCAPE_BIT;
	}

	// Check to see if the CRC is correct.
	require(self->mRXCRC == self->mRXCalcCRC, on_error);

	// This packet was received successfully!

	soot_handle_rx_packet(self);

	DP_EXIT(&self->mRXDataDP);

on_error:

	ERROR_PRINTF("soot:%p:soot_rx_data_parser: Corrupted stream at line %d", self, self->mRXDataDP.pt.lc);
	DEBUG_PRINTF("soot:%p:bad_rx_packet: TYPE:%s AFC:%d OFC:%d DecodedPayloadLen:%d RXCRC:0x%04x RXCalcCRC:0x%04x",
		self,
		soot_packet_type_to_string(self->mRXType),
		self->mRXAFC,
		self->mRXOFC,
		self->mRXLen,
		self->mRXCRC,
		self->mRXCalcCRC
	);

	if (self->mSootState == SOOT_STATE_RUNNING) {
		soot_signal_nack(self);
	}

	PT_WAIT_UNTIL(&self->mRXDataDP.pt, false);

	DP_END(&self->mRXDataDP); // LCOV_EXCL_LINE
}

void
soot_process_rx_data(soot_instance_t* self, const void *buf, size_t nbyte)
{
	SOOT_EMBEDDED_SELF_HOOK;

	while(nbyte > 0) {
		uint8_t byte = *(const uint8_t*)buf;

		size_t count = nbyte;
		const void* esc = memchr((const uint8_t*)buf+1, SOOT_BYTE_FLAG, nbyte-1);

		// If we found an instance of SOOT_BYTE_FLAG that wasn't
		// the first byte, then trim all of those bytes off of
		// the buffer we hand to `soot_rx_data_parser()`.
		// This way we ensure that `soot_rx_data_parser()` is always
		// properly reset when parsing a new frame.
		if (esc) {
			count = (size_t)((const uint8_t*)esc - (const uint8_t*)buf);
		}

		if (byte == SOOT_BYTE_FLAG) {
			DP_INIT(&self->mRXDataDP);
		}

#if SOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_FACTOR
		count = 1;
		if (quick_dirty_insecure_random(SOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_WINDOW) < SOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_FACTOR) {
			// Insert a random byte into the stream to corrupt it
			byte = quick_dirty_insecure_random(256);
			soot_rx_data_parser(self, &byte, 1);
		}
#endif

		soot_rx_data_parser(self, buf, count);

		buf = (const uint8_t*)buf + count;
		nbyte -= count;
	}
}

// ----------------------------------------------------------------------------
// MARK: -
// MARK: Wrting to the Stream

bool
soot_can_write(const soot_instance_t* self)
{
	SOOT_EMBEDDED_SELF_HOOK;
	if (self->mDidReset || (self->mSootState != SOOT_STATE_RUNNING)) {
		return false;
	}

	return soot_next_writable_tx_buffer((soot_instance_t*)self) != NULL;
}

ssize_t
soot_write(soot_instance_t* self, const void *buf, size_t nbyte)
{
	SOOT_EMBEDDED_SELF_HOOK;
	ssize_t written = 0;
	soot_tx_buffer_t *buffer;
	const uint8_t* data = buf;

	if (self->mSootState != SOOT_STATE_RUNNING) {
		goto bail;
	}

	if (self->mDidReset) {
		written = -1;
		goto bail;
	}

	while (nbyte && (buffer = soot_next_writable_tx_buffer(self))) {
		if (buffer->mState == SOOT_TX_BUFFER_STATE_ACKNOWLEGED) {
			soot_outbound_frame_recycle(self, buffer);
		}

		if (buffer != soot_current_tx_buffer(self)) {
			self->mHotBuffer = buffer;
		} else {
			soot_tx_buffer_t *prev = soot_prev_tx_buffer(self);
			if (prev->mState == SOOT_TX_BUFFER_STATE_ACKNOWLEGED) {
				soot_outbound_frame_recycle(self, prev);
			}
		}

		if (buffer->mState == SOOT_TX_BUFFER_STATE_ACKNOWLEGED) {
			soot_outbound_frame_recycle(self, buffer);
		}

		for (; nbyte != 0; nbyte--, data++) {
			if (soot_byte_needs_escape(*data)) {
				if (buffer->mData.mFrame.mPayloadLen >= SOOT_MAX_PAYLOAD_SIZE - 1) {
					break;
				}
				buffer->mData.mFrame.mPayload[buffer->mData.mFrame.mPayloadLen++] = SOOT_BYTE_ESC;
				buffer->mData.mFrame.mPayload[buffer->mData.mFrame.mPayloadLen++] = (*data ^ SOOT_ESCAPE_XFORM);
			} else {
				if (buffer->mData.mFrame.mPayloadLen >= SOOT_MAX_PAYLOAD_SIZE) {
					break;
				}
				buffer->mData.mFrame.mPayload[buffer->mData.mFrame.mPayloadLen++] = *data;
			}
			written++;
		}

		assert(buffer->mData.mFrame.mPayloadLen <= SOOT_MAX_PAYLOAD_SIZE);
	}

	DEBUG_PRINTF("soot:%p: BytesWritten: %d (%d remaining)", self, (int)written, (int)nbyte);

bail:
	return written;
}
