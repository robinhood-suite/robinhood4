#!/bin/sh
#
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if [ $# -lt 1 ]
then
    echo "Invalid number of arguments."
    echo "Usage: make_rpm.sh <builddir>"
    exit 1
fi

BUILDDIR=$1

. "$BUILDDIR/packaging/build_options.sh"

meson dist -C "$BUILDDIR" --no-tests

if [ "$BUILD_LUSTRE" == "False" ]; then
    opts+="--without lustre "
fi
if [ "$BUILD_MFU" == "True" ]; then
    opts+="--with mfu "
fi
if [ "$BUILD_HESTIA" == "True" ]; then
    opts+="--with hestia "
fi

rpmbuild --define="_topdir $PWD/rpms" $opts \
    -ta "$BUILDDIR/meson-dist/robinhood4-$VERSION.tar.xz"
