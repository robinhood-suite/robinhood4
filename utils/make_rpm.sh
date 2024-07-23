#!/bin/sh

if [ $# -lt 2 ]
then
    echo "Invalid number of arguments."
    echo "Usage: make_rpm.sh <builddir> <version>"
    exit 1
fi

BUILDDIR=$1
VERSION=$2
BUILD_MPI=$3

ninja -C "$BUILDDIR" dist

# This is not generic at all, but it will suffice for now
if [ "$BUILD_MPI" == "true" ]; then
    rpmbuild --define="_topdir $PWD/rpms" --with mfu \
        -ta "$BUILDDIR/meson-dist/robinhood4-$VERSION.tar.xz"
else
    rpmbuild --define="_topdir $PWD/rpms" \
        -ta "$BUILDDIR/meson-dist/robinhood4-$VERSION.tar.xz"
fi
