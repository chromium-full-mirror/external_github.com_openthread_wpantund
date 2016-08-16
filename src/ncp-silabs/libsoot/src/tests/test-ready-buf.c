#ifndef ASSERT_REQUIRE_FAILURE_HOOK
#define ASSERT_REQUIRE_FAILURE_HOOK     abort()
#endif

#include "test-harness.h"

int
main(void)
{
	soot_instance_t soot_a, soot_b;
	test_harness_data harness_a = {}, harness_b = {};
	int i;
	int max_iter = MAX_ITERATIONS+1;

	soot_init(&soot_a);
	soot_init(&soot_b);

	connect_test_harness(&soot_a, &harness_a);
	connect_test_harness(&soot_b, &harness_b);

	for(i = 0; i != max_iter; i++) {
		uint8_t buffer[SOOT_SMALL_FIFO_SIZE/4+2];
		ssize_t read_bytes;

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
		ERROR_PRINTF("Did not converge after after %d iterations (Max %d)", i, max_iter-1);
	}


	if (i < MIN_ITERATIONS) {
		ERROR_PRINTF("Convergence took fewer than %d iterations, this is suspicious", i);
		exit(EXIT_FAILURE);
	}

// **** LCOV_EXCL_STOP ****

	soot_finalize(&soot_a);
	soot_finalize(&soot_b);

	return i != max_iter
		? EXIT_SUCCESS
		: EXIT_FAILURE;
}

// **** LCOV_EXCL_STOP ****
