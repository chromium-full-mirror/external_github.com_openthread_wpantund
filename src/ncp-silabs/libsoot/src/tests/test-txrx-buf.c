#ifndef ASSERT_REQUIRE_FAILURE_HOOK
#define ASSERT_REQUIRE_FAILURE_HOOK     abort()
#endif

#include "test-harness.h"

int
main(void)
{
	int ret = 1;
	soot_instance_t soot_a, soot_b;
	test_harness_data harness_a = {}, harness_b = {};
	int i;
	static const char test_string[] = "~}\x13\x11~}\x13\x11~}\x13\x11~}\x13\x11~}\x13\x11~}\x13\x11~}\x13\x11";
	char read_string[sizeof(test_string)*2];
	ssize_t len = 0;
	uint8_t buffer[SOOT_SMALL_FIFO_SIZE/4+3];
	ssize_t read_bytes;
	int max_iter = MAX_ITERATIONS+1;

	soot_init(&soot_a);
	soot_init(&soot_b);

	connect_test_harness(&soot_a, &harness_a);
	connect_test_harness(&soot_b, &harness_b);

	for(i = 0; i != max_iter; i++) {
		INCREMENT_TIME_MS(20);

		if (soot_wants_to_tx_data(&soot_a)) {
			soot_process_tx_data(&soot_a);
		}

		if (soot_wants_to_tx_data(&soot_b)) {
			soot_process_tx_data(&soot_b);
		}

		read_bytes = soot_fifo_read(&harness_a.tx_fifo, buffer, sizeof(buffer));
		soot_process_rx_data(&soot_b, buffer, read_bytes);

		read_bytes = soot_fifo_read(&harness_b.tx_fifo, buffer, sizeof(buffer));
		soot_process_rx_data(&soot_a, buffer, read_bytes);

		require_noerr_action(soot_get_last_status(&soot_b), bail, ret = 1);
		require_noerr_action(soot_get_last_status(&soot_a), bail, ret = 1);

		if ((soot_a.mSootState == SOOT_STATE_RUNNING)
			&& (soot_b.mSootState == SOOT_STATE_RUNNING)
		) {
			break;
		}
	}

// **** LCOV_EXCL_START ****
	if (i != max_iter) {
		DEBUG_PRINTF("Converged after %d iterations", i);
	} else {
		DEBUG_PRINTF("Did not converge after after %d iterations (Max %d)", i, max_iter-1);
	}
// **** LCOV_EXCL_STOP ****

	len = soot_write(&soot_a,test_string,strlen(test_string));
	require_action(len == strlen(test_string), bail, ret = 1);

	for(i = 0; i != max_iter; i++) {
		INCREMENT_TIME_MS(20);

		if (soot_wants_to_tx_data(&soot_a)) {
			soot_process_tx_data(&soot_a);
		}


		if (soot_wants_to_tx_data(&soot_b)) {
			soot_process_tx_data(&soot_b);
		}

		read_bytes = soot_fifo_read(&harness_a.tx_fifo, buffer, sizeof(buffer));
		soot_process_rx_data(&soot_b, buffer, read_bytes);

		read_bytes = soot_fifo_read(&harness_b.tx_fifo, buffer, sizeof(buffer));
		soot_process_rx_data(&soot_a, buffer, read_bytes);

		len = soot_fifo_read(&harness_b.read_fifo, (uint8_t*)read_string, sizeof(read_string));

		require_action(len>=0, bail, ret = 1);

		require_noerr_action(soot_get_last_status(&soot_b), bail, ret = 1);
		require_noerr_action(soot_get_last_status(&soot_a), bail, ret = 1);

		if(len>0) {
			DEBUG_DUMP(&soot_b, "soot_read_b", read_string, len);
			if(strlen(test_string)==len && (0==strncmp(test_string,read_string,len))) {
				ret = 0;
			}
			break;
		}
	}

// **** LCOV_EXCL_START ****
	if (i != max_iter) {
		DEBUG_PRINTF("Converged after %d iterations", i);

		if (strlen(test_string)!=len || (0!=strncmp(test_string,read_string,len))) {
			DEBUG_PRINTF("String mismatch");
			DEBUG_DUMP(&soot_b, "test_string", test_string, sizeof(test_string));
			DEBUG_DUMP(&soot_b, "read_string", read_string, len);
		}
	} else {
		DEBUG_PRINTF("Did not converge after after %d iterations (Max %d)", i, max_iter-1);
	}
// **** LCOV_EXCL_STOP ****

	require(ret == 0, bail);

	ret = 1;
	len = soot_write(&soot_b,test_string,strlen(test_string));
	require_action(len == strlen(test_string), bail, ret = 1);

	for(i = 0; i != max_iter; i++) {
		INCREMENT_TIME_MS(20);

		if (soot_wants_to_tx_data(&soot_a)) {
			soot_process_tx_data(&soot_a);
		}



		if (soot_wants_to_tx_data(&soot_b)) {
			soot_process_tx_data(&soot_b);
		}

		read_bytes = soot_fifo_read(&harness_a.tx_fifo, buffer, sizeof(buffer));
		soot_process_rx_data(&soot_b, buffer, read_bytes);

		read_bytes = soot_fifo_read(&harness_b.tx_fifo, buffer, sizeof(buffer));
		soot_process_rx_data(&soot_a, buffer, read_bytes);

		len = soot_fifo_read(&harness_a.read_fifo, (uint8_t*)read_string, sizeof(read_string));

		require_action(len>=0, bail, ret = 1);

		require_noerr_action(soot_get_last_status(&soot_b), bail, ret = 1);
		require_noerr_action(soot_get_last_status(&soot_a), bail, ret = 1);

		if(len>0) {
			DEBUG_DUMP(&soot_b, "soot_read_a", read_string, len);
			if(strlen(test_string)==len && (0==strncmp(test_string,read_string,len))) {
				ret = 0;
			}
			break;
		}
	}

// **** LCOV_EXCL_START ****
	if (i != max_iter) {
		DEBUG_PRINTF("Converged after %d iterations", i);
	} else {
		DEBUG_PRINTF("Did not converge after after %d iterations (Max %d)", i, max_iter-1);
	}

	if (i < MIN_ITERATIONS) {
		ERROR_PRINTF("Convergence took fewer than %d iterations, this is suspicious", i);
		exit(EXIT_FAILURE);
	}
// **** LCOV_EXCL_STOP ****

bail:
	soot_finalize(&soot_a);
	soot_finalize(&soot_b);

	return ret == 0
		? EXIT_SUCCESS
		: EXIT_FAILURE;
}
