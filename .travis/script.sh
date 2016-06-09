#!/bin/sh
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

PREV_PATH="`pwd`"

die() {
	echo " *** ERROR: " $*
	exit 1
}

set -x

[ $TRAVIS_OS_NAME != linux ] || BUILD_CONFIGFLAGS="${BUILD_CONFIGFLAGS} --with-connman"

[ -e configure ] || ./bootstrap.sh || die

mkdir -p "${BUILD_MAKEPATH}" || die

cd "${BUILD_MAKEPATH}" || die

../configure ${BUILD_CONFIGFLAGS} || die

make ${BUILD_MAKEARGS} || die

cd ..
