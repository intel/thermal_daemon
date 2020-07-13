#!/bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.
olddir=`pwd`

cd "$srcdir"

aclocal --install || exit 1
gtkdocize --copy --flavour no-tmpl || exit 1
autoreconf --install --verbose || exit 1

cd "$olddir"

if test -z "$NO_CONFIGURE"; then
    $srcdir/configure "$@" && echo "Now type 'make' to compile `basename $srcdir`."
fi
