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

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/third_party/pt -I$(top_srcdir)/third_party/assert-macros
DISTCLEANFILES = .deps Makefile

if ALLOW_INSTALL
pkgincludedir = $(includedir)/soot
pkginclude_HEADERS = soot.h
lib_LTLIBRARIES = libsoot.la

libsoot_la_LDFLAGS = -version-info 0:0:0

# Ensure that `ldconfig` gets called on platforms that need it.
# We ignore the return value in case it doesn't exist, like on OS X.
install-exec-hook:
	-[ "`whoami`" = "root" ] && ldconfig 2> /dev/null
else
noinst_LTLIBRARIES = libsoot.la
endif

libsoot_la_SOURCES = soot.c soot.h dp.h soot-debug.h soot-time.h soot-internal.h
