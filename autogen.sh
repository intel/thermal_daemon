#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.
REQUIRED_AUTOMAKE_VERSION=1.9
PKG_NAME=thermald

echo "looking for $srcdir "
(test -f $srcdir/configure.ac \
  && test -f $srcdir/src/main.c) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

(cd $srcdir;
    autopoint --force
    AUTOPOINT='intltoolize --automake --copy' autoreconf --force --install --verbose
)

if test -z "$NOCONFIGURE"; then
	$srcdir/configure --enable-maintainer-mode "$@"
fi
