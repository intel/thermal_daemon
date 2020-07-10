#!/bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.
olddir=`pwd`

cd "$srcdir"

autoreconf --install --verbose

cd "$olddir"
