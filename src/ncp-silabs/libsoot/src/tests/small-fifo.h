#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef SOOT_SMALL_FIFO_SIZE
#define SOOT_SMALL_FIFO_SIZE	6
#endif

#ifndef SOOT_RX_FIFO_SCALE
#define SOOT_RX_FIFO_SCALE         (7)
#endif

#define SOOT_RX_FIFO_SIZE          (1 << SOOT_RX_FIFO_SCALE)
#define SOOT_RX_FIFO_MASK          (SOOT_RX_FIFO_SIZE - 1)

// A structure for a simple ring buffer
typedef struct {
	// All member variables defined below are PRIVATE.
	uint8_t mBuffer[SOOT_RX_FIFO_SIZE];
	uint8_t mReadIdx, mWriteIdx;
} soot_fifo_t;

// ----------------------------------------------------------------------------
// MARK: -
// MARK: Simple FIFO Implementation

static int
soot_fifo_space_used(const soot_fifo_t* self)
{
	int ret;

	if (self->mWriteIdx >= self->mReadIdx) {
		ret = self->mWriteIdx - self->mReadIdx;
	} else {
		ret = SOOT_RX_FIFO_SIZE + self->mWriteIdx - self->mReadIdx;
	}

	return ret;
}

static int
soot_fifo_space_available(const soot_fifo_t* self)
{
	return SOOT_RX_FIFO_SIZE - 1 - soot_fifo_space_used(self);
}

static ssize_t
soot_fifo_write(soot_fifo_t* self, const uint8_t* buf, size_t nbytes)
{
	uint8_t bytes_to_write = soot_fifo_space_available(self);

	if (bytes_to_write < nbytes) {
		nbytes = bytes_to_write;
	} else {
		bytes_to_write = nbytes;
	}

	while(nbytes--) {
		self->mBuffer[self->mWriteIdx] = *buf++;
		self->mWriteIdx++;
		self->mWriteIdx &= SOOT_RX_FIFO_MASK;
	}

	return bytes_to_write;
}

static ssize_t
soot_fifo_read(soot_fifo_t* self, uint8_t* buf, size_t nbytes)
{
	ssize_t bytes_to_read = soot_fifo_space_used(self);

	if (bytes_to_read < nbytes) {
		nbytes = bytes_to_read;
	} else {
		bytes_to_read = nbytes;
	}

	while(nbytes--) {
		*buf++ = self->mBuffer[self->mReadIdx];
		self->mReadIdx++;
		self->mReadIdx &= SOOT_RX_FIFO_MASK;
	}

	if(self->mReadIdx == self->mWriteIdx) {
		self->mReadIdx = self->mWriteIdx = 0;
	}

	return bytes_to_read;
}




static int
soot_small_fifo_space_used(const soot_fifo_t* self)
{
	int ret;

	if (self->mWriteIdx >= self->mReadIdx) {
		ret = self->mWriteIdx - self->mReadIdx;
	} else {
		ret = SOOT_SMALL_FIFO_SIZE + self->mWriteIdx - self->mReadIdx;
	}

	assert(ret >= 0);
	assert(ret < SOOT_SMALL_FIFO_SIZE);

	return ret % SOOT_SMALL_FIFO_SIZE;
}

static int
soot_small_fifo_space_available(const soot_fifo_t* self)
{
	return SOOT_SMALL_FIFO_SIZE - 1 - soot_small_fifo_space_used(self);
}

static ssize_t
soot_small_fifo_write(soot_fifo_t* self, const uint8_t* buf, size_t nbytes)
{
	uint8_t bytes_to_write = soot_small_fifo_space_available(self);

	if (bytes_to_write < nbytes) {
		nbytes = bytes_to_write;
	}

	bytes_to_write = nbytes;

	while(nbytes--) {
		self->mBuffer[self->mWriteIdx] = *buf++;
		self->mWriteIdx++;
		self->mWriteIdx %= SOOT_SMALL_FIFO_SIZE;
	}

	return bytes_to_write;
}

static ssize_t
soot_small_fifo_read(soot_fifo_t* self, uint8_t* buf, size_t nbytes)
{
	ssize_t bytes_to_read = soot_small_fifo_space_used(self);

	if (bytes_to_read < nbytes) {
		nbytes = bytes_to_read;
	}

	bytes_to_read = nbytes;

	while(nbytes--) {
		*buf++ = self->mBuffer[self->mReadIdx];
		self->mReadIdx++;
		self->mReadIdx %= SOOT_SMALL_FIFO_SIZE;
	}

	return bytes_to_read;
}
