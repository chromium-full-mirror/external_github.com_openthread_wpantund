// Protothread Data Parser Extension

#ifndef __pt_dataparser_header__
#define __pt_dataparser_header__

#include "pt.h"

struct dp {
	struct pt pt;
	uint16_t read_remaining;
};

#define DP_INIT(dp)		PT_INIT(&(dp)->pt)

#define DP_EXIT(dp)		PT_EXIT(&(dp)->pt)

// Note: These macros assume that the ingest
//       function will have the arguments "bytes"
//       and "len".
#define DP_BEGIN(dp)    PT_BEGIN(&(dp)->pt)

#define DP_END(dp)      PT_END(&(dp)->pt)

#define DP_READ_BYTE(dp, byte_ptr) do { \
		PT_WAIT_UNTIL(&(dp)->pt, len>=1); \
		*(byte_ptr) = *bytes++; \
		len--; \
	} while (0)

#define DP_READ_DATA(dp, data_ptr, data_len) do { \
		(dp)->read_remaining = (data_len); \
		while ((dp)->read_remaining != 0) { \
			DP_READ_BYTE(dp, (data_ptr) + (data_len) - (dp)->read_remaining); \
			(dp)->read_remaining--; \
		}; \
	} while (0)

#define DP_THREAD(name, context) PT_THREAD(name(context, const uint8_t* bytes, size_t len))

#endif
