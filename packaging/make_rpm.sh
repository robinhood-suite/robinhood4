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

# Always pass the explicit state of every build option to rpmbuild, so the
# RPM matches the meson configuration whatever the spec defaults are. Accept
# both boolean spellings for the BUILD_* values substituted by configure_file.
case "$BUILD_LDISKFS" in
    true|True) opts+="--with ldiskfs " ;;
    *) opts+="--without ldiskfs " ;;
esac
case "$BUILD_LUSTRE" in
    true|True) opts+="--with lustre " ;;
    *) opts+="--without lustre " ;;
esac
case "$BUILD_MAN" in
    true|True) opts+="--with man " ;;
    *) opts+="--without man " ;;
esac
case "$BUILD_MFU" in
    true|True) opts+="--with mfu " ;;
    *) opts+="--without mfu " ;;
esac
case "$BUILD_MPI" in
    true|True) opts+="--with mpi " ;;
    *) opts+="--without mpi " ;;
esac
case "$BUILD_MONGO" in
    true|True) opts+="--with mongo " ;;
    *) opts+="--without mongo " ;;
esac
case "$BUILD_S3" in
    true|True) opts+="--with s3 " ;;
    *) opts+="--without s3 " ;;
esac
case "$BUILD_SELINUX" in
    true|True) opts+="--with selinux " ;;
    *) opts+="--without selinux " ;;
esac
case "$BUILD_ACL" in
    true|True) opts+="--with acl " ;;
    *) opts+="--without acl " ;;
esac

rpmbuild --define="_topdir $PWD/rpms" $opts \
    -ta "$BUILDDIR/meson-dist/robinhood4-$VERSION.tar.xz"
