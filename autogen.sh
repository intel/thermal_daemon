#!/bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.
olddir=`pwd`

enable_doc=no
for arg in "$@"; do
    case "$arg" in
        --enable-doc|--enable-doc=yes|--enable-gtk-doc|--enable-gtk-doc=yes)
            enable_doc=yes
            ;;
    esac
done

cd "$srcdir"

aclocal --install || exit 1
if test "x$enable_doc" = "xyes"; then
    echo "autogen.sh: gtk-doc enabled; running gtkdocize"
    gtkdocize --copy --flavour no-tmpl || exit 1
else
    echo "autogen.sh: gtk-doc disabled; skipping gtkdocize"
fi
autoreconf --install --verbose || exit 1

cd "$olddir"

if test -z "$NO_CONFIGURE"; then
    $srcdir/configure "$@" && echo "Now type 'make' to compile `basename $srcdir`."
fi
