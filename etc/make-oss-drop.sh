#!/bin/sh

DIR="`dirname $0`"

if test x"$PACKAGE" = x
then PACKAGE=wpantund-oss
fi

echo PACKAGE="${PACKAGE}"

rm -fr "${DIR}/../make-oss-drop"

mkdir -p "${DIR}/../make-oss-drop"
cd "${DIR}/../make-oss-drop"

TMPFILE="`mktemp -t $(basename $0)`"

export LC_CTYPE=C
export LANG=C

( which -s diff ) && COMPARE_FILES="diff -q"
( which -s cmp ) && COMPARE_FILES="cmp"

die () {
	echo
	echo " *** Fatal error: $*"
	echo
	rm "${TMPFILE}"
	exit 1
}

test x"$COMPARE_FILES" = x && die "Can't find \"diff\" or \"cmp\""

../bootstrap.sh || die
../configure || die

make distdir=distdir distdir || die
cd distdir || die

# Remove all restricted plugins.
for filename in `find . -type f | grep -v '\.pdf$'`
do
	cat "$filename" |
		awk 'BEGIN {f=1} /begin pe8qf1P4GCme$/ {f=0} /end pe8qf1P4GCme$/ {f=1} f' |
		sed '/pe8qf1P4GCme$/d' > "${TMPFILE}" || die
	$COMPARE_FILES "$filename" "$TMPFILE" || ( cp "$TMPFILE" "$filename" || die "Unable to copy file" )
done

#cat "wpantund.xcodeproj/project.pbxproj" |
#	sed '
#/.*silabs.*[;,]$/d;
#/.*[Cc][Oo][Nn][Nn][Ee][Cc][Tt][Ii][Pp].*[;,]$/d;
#/.*[Ss]oot.*[;,]$/d;
#/.*[Ee]mber.*[;,]$/d;
#/.*ash-.*[;,]$/d;
#s:\(/\*.*silabs.*\*/\):/*pe8qf1P4GCme*/:g;
#' > "${TMPFILE}" || die
#$COMPARE_FILES "wpantund.xcodeproj/project.pbxproj" "$TMPFILE" || ( cp "$TMPFILE" "wpantund.xcodeproj/project.pbxproj" || die "Unable to copy file" )



rm "${TMPFILE}"

# Re-run the bootstrap and reconfigure
./bootstrap.sh || die
./configure || die

make -j20 distcheck || die

make PACKAGE="${PACKAGE}" dist dist-bzip2 || die

mv *.tar.gz *.bz2 ../../ || die

cd ../..

rm -fr "make-oss-drop"
