#if HAVE_CONFIG_H
#include <config.h>
#undef HAVE_CONFIG_H
#endif

#undef ASSERT_MACROS_USE_SYSLOG
#define ASSERT_MACROS_USE_SYSLOG 0

#define DEBUG 1
#define VERBOSE_DEBUG 1

#include <stdlib.h>

static int32_t gCurrentTimeMS;
static int32_t soot_test_hook_get_ms(void) { return gCurrentTimeMS; }
#define SOOT_CONFIG_HOOK_GET_MS soot_test_hook_get_ms()

#include "../soot/soot.c"

#include "small-fifo.h"

#include <stdlib.h>
#include <unistd.h>

#ifndef MAX_ITERATIONS
#define MAX_ITERATIONS	20
#endif

#ifndef MIN_ITERATIONS
#define MIN_ITERATIONS	0
#endif

typedef struct {
	soot_fifo_t read_fifo;
	soot_fifo_t tx_fifo;
} test_harness_data;

static ssize_t
soot_adapter_tx_callback(void* tx_data_context, const void *buf, size_t nbyte)
{
	ssize_t ret;
	test_harness_data* wrapper = (test_harness_data*)tx_data_context;

	ret = soot_fifo_write(&wrapper->tx_fifo, buf, nbyte);

	DEBUG_DUMP(NULL, "soot_adapter_tx_callback", buf, ret>=0?ret:0);

	return ret;
}

static ssize_t
soot_adapter_read_callback(void* read_data_context, const void *buf, size_t nbyte)
{
	test_harness_data* wrapper = (test_harness_data*)read_data_context;

	DEBUG_DUMP(NULL, "soot_adapter_read_callback", buf, nbyte);

	if (nbyte > soot_fifo_space_available(&wrapper->read_fifo)) {
		return 0;
	}

	return soot_fifo_write(&wrapper->read_fifo, buf, nbyte);
}

static test_harness_data gHarness;

static void
connect_test_harness(soot_instance_t* soot, test_harness_data* harness)
{
#if !SOOT_EMBEDDED
	soot_set_callbacks(soot, soot_adapter_tx_callback, soot_adapter_read_callback, harness);
#endif
}

#if SOOT_EMBEDDED
ssize_t
soot_hook_tx_data(soot_instance_t* self, const void *buf, size_t nbyte)
{
	return soot_adapter_tx_callback(&gHarness, buf, nbyte);
}
ssize_t
soot_hook_read_data(soot_instance_t* self, const void *buf, size_t nbyte)
{
	return soot_adapter_read_callback(&gHarness, buf, nbyte);
}
#endif


#define BEGIN_TEST() \
	int ret = 1; \
	soot_instance_t *soot_instance = soot_create(); \
	require(soot_instance != NULL,bail); \
	connect_test_harness(soot_instance, &gHarness);

#define END_TEST() \
	ret = 0; \
	bail: \
		soot_release(soot_instance); \
		return ret == 0 \
			? EXIT_SUCCESS \
			: EXIT_FAILURE

#define INCREMENT_TIME_MS(x) do { \
		fprintf(stderr,"INCREMENT_TIME_MS:%d Incremented %d ms\n", __LINE__, (x)); \
		gCurrentTimeMS += (x); \
	} while(0)

#define PROCESS_RX_DATA(...) \
	do { \
		const uint8_t data[] = __VA_ARGS__; \
		fprintf(stderr,"PROCESS_RX_DATA:%d\n", __LINE__); \
		soot_process_rx_data(soot_instance, data, sizeof(data)); \
	} while(0)

#define PROCESS_RX_PACKET(type,ofc,afc,...) \
	do { \
		const uint8_t data[] = __VA_ARGS__; \
		soot_tx_buffer_t buffer = { }; \
		assert(sizeof(data)<=SOOT_MAX_PAYLOAD_SIZE); \
		soot_tx_buffer_reset(&buffer); \
		soot_tx_buffer_set_type(&buffer, type); \
		soot_tx_buffer_set_ofc(&buffer, ofc); \
		soot_tx_buffer_set_afc(&buffer, afc); \
		buffer.mData.mFrame.mPayloadLen = sizeof(data); \
		memcpy(buffer.mData.mFrame.mPayload, data, sizeof(data)); \
		soot_tx_buffer_prep_for_flush(&buffer); \
		soot_tx_buffer_recalc_crc(&buffer); \
		fprintf(stderr,"PROCESS_RX_PACKET:%d t:%s ofc:%d afc:%d plen:%d\n", __LINE__, soot_packet_type_to_string(type), ofc, afc, (int)sizeof(data)); \
		soot_tx_buffer_DEBUG_DUMP(&buffer,"PROCESS_RX_PACKET",false); \
		soot_process_rx_data(soot_instance, buffer.mData.mRaw, buffer.mLen); \
	} while(0)

#define VERIFY_TX_DATA(...) \
	do { \
		const uint8_t data[] = __VA_ARGS__; \
		assert(sizeof(data)<=SOOT_MAX_PAYLOAD_SIZE); \
		uint8_t buffer[sizeof(data)]; \
		fprintf(stderr,"VERIFY_TX_DATA:%d\n", __LINE__); \
		soot_process_tx_data(soot_instance); \
		require(soot_get_last_status(soot_instance)==SOOT_STATUS_OK, bail); \
		require(soot_fifo_read(&gHarness.tx_fifo, buffer, sizeof(buffer))==sizeof(data), bail); \
		require(memcmp(data, buffer, sizeof(data))==0,bail); \
	} while(0)

#define VERIFY_NEXT_TIMEOUT(x) require(soot_get_timeout(soot_instance) == (x), bail)

#define VERIFY_TX_PACKET(type,ofc,afc,...) \
	do { \
		const uint8_t data[] = __VA_ARGS__; \
		uint8_t read_buffer[SOOT_MAX_PACKET_SIZE]; \
		soot_tx_buffer_t buffer = { }; \
		soot_tx_buffer_reset(&buffer); \
		soot_tx_buffer_set_type(&buffer, type); \
		soot_tx_buffer_set_ofc(&buffer, ofc); \
		soot_tx_buffer_set_afc(&buffer, afc); \
		buffer.mData.mFrame.mPayloadLen = sizeof(data); \
		memcpy(buffer.mData.mFrame.mPayload, data, sizeof(data)); \
		soot_tx_buffer_prep_for_flush(&buffer); \
		soot_tx_buffer_recalc_crc(&buffer); \
		fprintf(stderr,"VERIFY_TX_PACKET:%d t:%s ofc:%d afc:%d plen:%d\n", __LINE__, soot_packet_type_to_string(type), ofc, afc, (int)sizeof(data)); \
		soot_tx_buffer_DEBUG_DUMP(&buffer,"VERIFY_TX_PACKET",false); \
		soot_process_tx_data(soot_instance); \
		require(soot_get_last_status(soot_instance)==SOOT_STATUS_OK, bail); \
		require(soot_fifo_read(&gHarness.tx_fifo, read_buffer, buffer.mLen)==buffer.mLen, bail); \
		require(memcmp(buffer.mData.mRaw, read_buffer, buffer.mLen)==0,bail); \
	} while(0)

#define REQUIRE_EMPTY_TX() \
	do { \
		fprintf(stderr,"REQUIRE_EMPTY_TX:%d\n", __LINE__); \
		require(soot_wants_to_tx_data(soot_instance)==false, bail); \
		require(soot_fifo_space_used(&gHarness.tx_fifo)==0, bail); \
	} while(0)


#define FLUSH_TX_DATA() \
	do { \
		fprintf(stderr,"FLUSH_TX_DATA:%d\n", __LINE__); \
		gHarness.tx_fifo.mWriteIdx = gHarness.tx_fifo.mReadIdx = 0; \
	} while(0)

#define REQUIRE_WRITE_BLOCKED() \
	do { \
		static const uint8_t byte = 0; \
		require(!soot_can_write(soot_instance),bail); \
		require(soot_write(soot_instance, &byte, 1)<=0,bail); \
	} while(0)

#define WRITE_DATA(...) \
	do { \
		static const uint8_t data[] = __VA_ARGS__; \
		fprintf(stderr,"WRITE_DATA:%d\n", __LINE__); \
		require(soot_can_write(soot_instance),bail); \
		require(soot_write(soot_instance, data, sizeof(data))==sizeof(data),bail); \
	} while(0)

#define READ_DATA(...) \
	do { \
		static const uint8_t data[] = __VA_ARGS__; \
		uint8_t buffer[sizeof(data)]; \
		fprintf(stderr,"READ_DATA:%d\n", __LINE__); \
		DEBUG_DUMP(soot_instance, "EXPECTED", data, sizeof(data)); \
		require(soot_fifo_space_used(&gHarness.read_fifo)!=0,bail); \
		require(soot_fifo_read(&gHarness.read_fifo, buffer, sizeof(data))==sizeof(data),bail); \
		DEBUG_DUMP(soot_instance, "FOUND", buffer, sizeof(data)); \
		require(memcmp(data, buffer, sizeof(data))==0,bail); \
	} while(0)

#define REQUIRE_CONNECTED() require(soot_is_connected(soot_instance), bail)
#define REQUIRE_NOT_CONNECTED() require(!soot_is_connected(soot_instance), bail)

#define REQUIRE_RESET() require(soot_did_reset(soot_instance), bail)
