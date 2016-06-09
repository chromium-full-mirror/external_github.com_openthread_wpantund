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

die() {
	echo " *** ERROR: " $*
	exit 1
}

set -x

[ $TRAVIS_OS_NAME != linux ] || {
	sudo apt-get -y update || die
	sudo apt-get -y install bsdtar autoconf-archive automake autoconf || die
	wget https://gist.github.com/darconeous/d1d9bc39e0758e45a1d7/raw/ef9b01ac378a9b2e92031c846c4f6b5f94abab53/connman-include.tar.bz2 || die
	sudo tar xvjf connman-include.tar.bz2 -C / || die
}

[ $TRAVIS_OS_NAME != osx ] || {
	brew install d-bus || die
	brew install boost || die
	brew install autoconf-archive || die
}
