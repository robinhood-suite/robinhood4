#!/bin/sh
#
# This file is part of RobinHood
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

if [ $# -lt 1 ]
then
    echo "Invalid number of arguments."
    echo "Usage: make_rpm.sh <builddir>"
    exit 1
fi

BUILDDIR=$1

. "$BUILDDIR/packaging/build_options.sh"

meson dist -C "$BUILDDIR" --no-tests

if [ "$BUILD_LDISKFS" == "True" ]; then
    opts+="--with ldiskfs "
fi
if [ "$BUILD_LUSTRE" == "False" ]; then
    opts+="--without lustre "
fi
if [ "$BUILD_MAN" == "False" ]; then
    opts+="--without man "
fi
if [ "$BUILD_MFU" == "True" ]; then
    opts+="--with mfu "
fi
if [ "$BUILD_MPI" == "True" ]; then
    opts+="--with mpi "
fi
if [ "$BUILD_MONGO" == "False" ]; then
    opts+="--without mongo "
fi
if [ "$BUILD_S3" == "True" ]; then
    opts+="--with s3 "
fi
if [ "$BUILD_S3" == "True" ]; then
    opts+="--with s3 "
 fi

rpmbuild --define="_topdir $PWD/rpms" $opts \
    -ta "$BUILDDIR/meson-dist/robinhood4-$VERSION.tar.xz"
