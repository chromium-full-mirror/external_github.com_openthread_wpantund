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

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/util \
	-I$(top_srcdir)/src/ncp-silabs \
	-I$(top_srcdir)/src/wpantund \
	-I$(top_srcdir)/third_party/pt \
	-I$(top_srcdir)/third_party/assert-macros \
	-I$(top_srcdir)/src/ncp-silabs/libsoot/src \
	@CONNECTIP_CPPFLAGS@ \
	$(NULL)

EXTRA_DIST = \
    ../../third_party/connectip-1.1/LICENSE \
    ../../third_party/connectip-1.1/README.google \
    ../../third_party/connectip-1.1/app/ip-ncp/binary-command-identifiers.h \
    ../../third_party/connectip-1.1/stack/include/error.h \
    ../../third_party/connectip-1.1/stack/include/error-def.h \
    ../../third_party/connectip-1.1/stack/include/counter.h \
    ../../third_party/connectip-2.0/LICENSE \
    ../../third_party/connectip-2.0/README.google \
    ../../third_party/connectip-2.0/app/ip-ncp/binary-command-identifiers.h \
    ../../third_party/connectip-2.0/stack/include/error.h \
    ../../third_party/connectip-2.0/stack/include/error-def.h \
    ../../third_party/connectip-2.0/stack/include/counter.h \
    ../../third_party/silabs-thread-1.0/LICENSE \
    ../../third_party/silabs-thread-1.0/README.google \
    ../../third_party/silabs-thread-1.0/app/tmsp/tmsp-custom-enum.h \
    ../../third_party/silabs-thread-1.0/app/tmsp/tmsp-enum.h \
    ../../third_party/silabs-thread-1.0/stack/include/error.h \
    ../../third_party/silabs-thread-1.0/stack/include/error-def.h \
    ../../third_party/silabs-thread-1.0/stack/include/counter.h \
	$(NULL)

SUBDIRS = libsoot

# FirmwareInterface needs this for headers
AM_CPPFLAGS += -I$(top_srcdir)/third_party

DISTCLEANFILES = .deps Makefile

if BUILD_PLUGIN_NCP_SILABS

# Work-around the omnipotent automake nanny-state.
mypkglibexecdir = $(pkglibexecdir)

NCP_SOURCES = \
	EmberNCPControlInterface.cpp \
	EmberNCPControlInterface.h \
	EmberNCPInstance.cpp \
	EmberNCPInstance.h \
	EmberNCPUtils.h \
	EmberPacked.cpp \
	EmberPacked.h \
	FirmwareInterface.cpp \
	FirmwareInterface.h \
	FirmwareInterface-Base.cpp \
	FirmwareInterface-Base.h \
	FirmwareInterface-1.1.3.cpp \
	FirmwareInterface-1.1.3.h \
	FirmwareInterface-2.0.0.cpp \
	FirmwareInterface-2.0.0.h \
	FirmwareInterface-Thread-1.0.0.cpp \
	FirmwareInterface-Thread-1.0.0.h \
	EmberNCPControlInterface-Mfg.cpp \
	EmberNCPControlInterface-JoinTask.cpp \
	EmberNCPControlInterface-JoinTask.h \
	EmberNCPControlInterface-FormTask.cpp \
	EmberNCPControlInterface-FormTask.h \
	EmberNCPControlInterface-ScanTask.cpp \
	EmberNCPControlInterface-ScanTask.h \
	EmberNCPControlInterface-LurkTask.cpp \
	EmberNCPControlInterface-LurkTask.h \
	EmberNCPControlInterface-LeaveTask.cpp \
	EmberNCPControlInterface-LeaveTask.h \
	EmberNCPControlInterface-GetRIPEntryTask.cpp \
	EmberNCPControlInterface-GetRIPEntryTask.h \
	EmberNCPControlInterface-SendCommandTask.cpp \
	EmberNCPControlInterface-SendCommandTask.h \
	EmberNCPControlInterface-SetNetworkKeyTask.cpp \
	EmberNCPControlInterface-SetNetworkKeyTask.h \
	EmberNCPControlInterface-MfgFinish.cpp \
	EmberNCPControlInterface-MfgFinish.h \
	EmberNCPControlInterface-WakeupTask.cpp \
	EmberNCPControlInterface-WakeupTask.h \
	EmberNCPControlInterface-DeepSleepTask.cpp \
	EmberNCPControlInterface-DeepSleepTask.h \
	EmberNCPControlInterface-GetGlobalAddressesTask.cpp \
	EmberNCPControlInterface-GetGlobalAddressesTask.h \
	$(NULL)

if STATIC_LINK_NCP_PLUGIN
noinst_LTLIBRARIES = libncp-silabs.la
libncp_silabs_la_SOURCES = \
	$(NCP_SOURCES) \
	SootAdapter.cpp \
	SootAdapter.h \
	$(NULL)
libncp_silabs_la_LIBADD = libsoot/src/soot/libsoot.la
libncp_silabs_la_CXXFLAGS = @BOOST_CXXFLAGS@
else
mypkglibexec_LTLIBRARIES = ncp-silabs.la
ncp_silabs_la_LDFLAGS = \
	-module \
	-avoid-version \
	-shared \
	$(NULL)
ncp_silabs_la_SOURCES = \
	$(NCP_SOURCES) \
	SootAdapter.cpp \
	SootAdapter.h \
	$(NULL)
ncp_silabs_la_LIBADD = libsoot/src/soot/libsoot.la
ncp_silabs_la_CXXFLAGS = @BOOST_CXXFLAGS@
endif

endif #BUILD_PLUGIN_NCP_SILABS
