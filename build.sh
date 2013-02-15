#!/bin/bash +x
#
# Written by nicesj.park <nicesj.park@samsung.com>

BASE="."
CWD=`dirname $0`
cd "$CWD"

. ../../../../setup.conf || exit 1

echo "Current working directory is \"$CWD\""
if ! [ -d $BASE ]; then
	echo "This is not proper package"
	exit 1;
fi

cd $BASE

. ${TPLDIR}/cmake.tpl
run
