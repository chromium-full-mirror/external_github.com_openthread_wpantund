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

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src third_party

DISTCLEANFILES = config.log config.status Makefile libtool make.out

EXTRA_DIST = bootstrap.sh README.md LICENSE PROTOCOL.md

# Xcode Project Files
EXTRA_DIST += soot.xcodeproj/project.pbxproj

# Metafiles
EXTRA_DIST += .gitignore

@CODE_COVERAGE_RULES@
