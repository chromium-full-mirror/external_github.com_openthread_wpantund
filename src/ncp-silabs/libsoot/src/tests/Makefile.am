#
# Copyright (c) 2016 Nest Labs, Inc.
# All rights reserved.
#
# This document is the property of Nest. It is considered
# confidential and proprietary information.
#
# This document may not be reproduced or transmitted in any form,
# in whole or in part, without the express written permission of
# Nest.
#

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/third_party/pt -I$(top_srcdir)/third_party/assert-macros -DDEBUG=1
DISTCLEANFILES = .deps Makefile

TESTS =

@CODE_COVERAGE_RULES@

CFLAGS = $(CODE_COVERAGE_CFLAGS)
AM_LIBS = $(CODE_COVERAGE_LDFLAGS)

check_PROGRAMS = $(TESTS)

EXTRADIST = test-harness.h

TESTS += test_crc
test_crc_SOURCES = test-crc.c

TESTS += test_missing_rstack test_missing_rstack_embedded
test_missing_rstack_SOURCES = test-missing-rstack.c test-harness.h
test_missing_rstack_embedded_CPPFLAGS = $(AM_CPPFLAGS) -DSOOT_EMBEDDED=1
test_missing_rstack_embedded_SOURCES = test-missing-rstack.c test-harness.h

TESTS += test_init_01
test_init_01_SOURCES = test-init-01.c test-harness.h

TESTS += test_init_02
test_init_02_SOURCES = test-init-02.c test-harness.h

TESTS += test_reset_01
test_reset_01_SOURCES = test-reset-01.c test-harness.h

TESTS += test_reset_02
test_reset_02_SOURCES = test-reset-02.c test-harness.h

TESTS += test_escape_01
test_escape_01_SOURCES = test-escape-01.c test-harness.h

TESTS += test_nack_01
test_nack_01_SOURCES = test-nack-01.c test-harness.h

TESTS += test_nack_02
test_nack_02_SOURCES = test-nack-02.c test-harness.h

TESTS += test_outoforder_01
test_outoforder_01_SOURCES = test-outoforder-01.c test-harness.h


TESTS += test_rollover_01 test_rollover_01_embedded
test_rollover_01_SOURCES = test-rollover-01.c test-harness.h
test_rollover_01_embedded_CPPFLAGS = $(AM_CPPFLAGS) -DSOOT_EMBEDDED=1
test_rollover_01_embedded_SOURCES = test-rollover-01.c test-harness.h


TESTS += test_ready_buf70_crpt0
test_ready_buf70_crpt0_CPPFLAGS = $(AM_CPPFLAGS) -DSOOT_SMALL_FIFO_SIZE=70 -DSOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_FACTOR=0 -DMAX_ITERATIONS=1
test_ready_buf70_crpt0_SOURCES = test-ready-buf.c small-fifo.h

TESTS += test_ready_buf6_crpt0
test_ready_buf6_crpt0_CPPFLAGS = $(AM_CPPFLAGS) -DSOOT_SMALL_FIFO_SIZE=6 -DSOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_FACTOR=0 -DMAX_ITERATIONS=5
test_ready_buf6_crpt0_SOURCES = test-ready-buf.c small-fifo.h

TESTS += test_txrx_buf70_crpt0
test_txrx_buf70_crpt0_CPPFLAGS = $(AM_CPPFLAGS) -DSOOT_SMALL_FIFO_SIZE=70 -DSOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_FACTOR=0 -DMAX_ITERATIONS=3
test_txrx_buf70_crpt0_SOURCES = test-txrx-buf.c small-fifo.h

TESTS += test_txrx_buf6_crpt0
test_txrx_buf6_crpt0_CPPFLAGS = $(AM_CPPFLAGS) -DSOOT_SMALL_FIFO_SIZE=6 -DSOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_FACTOR=0 -DMAX_ITERATIONS=17
test_txrx_buf6_crpt0_SOURCES = test-txrx-buf.c small-fifo.h

TESTS += test_txrx_buf40_crpt1
test_txrx_buf40_crpt1_CPPFLAGS = $(AM_CPPFLAGS) -DSOOT_SMALL_FIFO_SIZE=40 -DSOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_FACTOR=1 -DASSERT_REQUIRE_FAILURE_HOOK= -DMAX_ITERATIONS=5  -DMIN_ITERATIONS=3
test_txrx_buf40_crpt1_SOURCES = test-txrx-buf.c small-fifo.h

TESTS += test_txrx_buf40_crpt3
test_txrx_buf40_crpt3_CPPFLAGS = $(AM_CPPFLAGS) -DSOOT_SMALL_FIFO_SIZE=40 -DSOOT_CONFIG_DEBUG_RX_STREAM_CORRUPT_FACTOR=3 -DASSERT_REQUIRE_FAILURE_HOOK= -DMAX_ITERATIONS=85 -DMIN_ITERATIONS=10
test_txrx_buf40_crpt3_SOURCES = test-txrx-buf.c small-fifo.h
