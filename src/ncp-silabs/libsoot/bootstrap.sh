#!/bin/sh

die() {
	echo
	echo " *** $1 failed with error code $?"
	exit 1
}

cd "`dirname "$0"`"

which autoreconf 1>/dev/null 2>&1 || {
	echo " *** error: The 'autoreconf' command was not found."
	echo "Use the appropriate command for your platform to install the package:"
	echo ""
	echo "Homebrew(OS X) ....... brew install libtool autoconf autoconf-archive"
	echo "Debian/Ubuntu ........ apt-get install libtool autoconf autoconf-archive"
	exit 1
}

AUTOMAKE="automake --foreign" autoreconf --verbose --force --install || die autoreconf

echo
echo Success.
