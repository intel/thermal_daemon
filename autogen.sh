#!/bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.
olddir=`pwd`

cd "$srcdir"

autoreconf --install --verbose

cd "$olddir"

if test -z "$NO_CONFIGURE"; then
    $srcdir/configure "$@" && echo "Now type 'make' to compile `basename $srcdir`."
fi
