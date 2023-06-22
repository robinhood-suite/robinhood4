/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>

#include <sys/stat.h>

#include "robinhood/statx.h"

#include "check-compat.h"

START_TEST(type)
{
    static_assert(RBH_STATX_TYPE == STATX_TYPE, "");
}
END_TEST

START_TEST(mode)
{
    static_assert(RBH_STATX_MODE == STATX_MODE, "");
}
END_TEST

START_TEST(nlink)
{
    static_assert(RBH_STATX_NLINK == STATX_NLINK, "");
}
END_TEST

START_TEST(uid)
{
    static_assert(RBH_STATX_UID == STATX_UID, "");
}
END_TEST

START_TEST(gid)
{
    static_assert(RBH_STATX_GID == STATX_GID, "");
}
END_TEST

START_TEST(atime)
{
    static_assert(RBH_STATX_ATIME_SEC == STATX_ATIME, "");
}
END_TEST

START_TEST(mtime)
{
    static_assert(RBH_STATX_MTIME_SEC == STATX_MTIME, "");
}
END_TEST

START_TEST(ctime)
{
    static_assert(RBH_STATX_CTIME_SEC == STATX_CTIME, "");
}
END_TEST

START_TEST(ino)
{
    static_assert(RBH_STATX_INO == STATX_INO, "");
}
END_TEST

START_TEST(size)
{
    static_assert(RBH_STATX_SIZE == STATX_SIZE, "");
}
END_TEST

START_TEST(blocks)
{
    static_assert(RBH_STATX_BLOCKS == STATX_BLOCKS, "");
}
END_TEST

START_TEST(btime)
{
    static_assert(RBH_STATX_BTIME_SEC == STATX_BTIME, "");
}
END_TEST

START_TEST(rbh_atime)
{
    static_assert(
        RBH_STATX_ATIME == (RBH_STATX_ATIME_SEC | RBH_STATX_ATIME_NSEC), ""
    );
}
END_TEST

START_TEST(rbh_btime)
{
    static_assert(
        RBH_STATX_BTIME == (RBH_STATX_BTIME_SEC | RBH_STATX_BTIME_NSEC), ""
    );
}
END_TEST

START_TEST(rbh_ctime)
{
    static_assert(
        RBH_STATX_CTIME == (RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC), ""
    );
}
END_TEST

START_TEST(rbh_mtime)
{
    static_assert(
        RBH_STATX_MTIME == (RBH_STATX_MTIME_SEC | RBH_STATX_MTIME_NSEC), ""
    );
}
END_TEST

START_TEST(rbh_rdev)
{
    static_assert(
        RBH_STATX_RDEV == (RBH_STATX_RDEV_MAJOR | RBH_STATX_RDEV_MINOR), ""
    );
}
END_TEST

START_TEST(rbh_dev)
{
    static_assert(
        RBH_STATX_DEV == (RBH_STATX_DEV_MAJOR | RBH_STATX_DEV_MINOR), ""
    );
}
END_TEST

START_TEST(rbh_basic_stats)
{
    static_assert(RBH_STATX_BASIC_STATS == (STATX_BASIC_STATS
                                          | RBH_STATX_BLKSIZE
                                          | RBH_STATX_ATIME_NSEC
                                          | RBH_STATX_CTIME_NSEC
                                          | RBH_STATX_MTIME_NSEC
                                          | RBH_STATX_RDEV_MAJOR
                                          | RBH_STATX_RDEV_MINOR
                                          | RBH_STATX_DEV_MAJOR
                                          | RBH_STATX_DEV_MINOR), "");
}
END_TEST

START_TEST(rbh_all)
{
    static_assert(RBH_STATX_ALL == (RBH_STATX_BASIC_STATS
                                  | RBH_STATX_ATTRIBUTES
                                  | RBH_STATX_BTIME_SEC
                                  | RBH_STATX_BTIME_NSEC
                                  | RBH_STATX_MNT_ID), "");
}
END_TEST

START_TEST(rbh_statx_size)
{
    static_assert(sizeof(struct rbh_statx) == sizeof(struct statx), "");
}
END_TEST

START_TEST(rbh_statx_fields)
{
    static_assert(offsetof(struct rbh_statx, stx_mask)
               == offsetof(struct statx, stx_mask), "");
    static_assert(offsetof(struct rbh_statx, stx_blksize)
               == offsetof(struct statx, stx_blksize), "");
    static_assert(offsetof(struct rbh_statx, stx_attributes)
               == offsetof(struct statx, stx_attributes), "");
    static_assert(offsetof(struct rbh_statx, stx_nlink)
               == offsetof(struct statx, stx_nlink), "");
    static_assert(offsetof(struct rbh_statx, stx_uid)
               == offsetof(struct statx, stx_uid), "");
    static_assert(offsetof(struct rbh_statx, stx_gid)
               == offsetof(struct statx, stx_gid), "");
    static_assert(offsetof(struct rbh_statx, stx_mode)
               == offsetof(struct statx, stx_mode), "");
    static_assert(offsetof(struct rbh_statx, stx_ino)
               == offsetof(struct statx, stx_ino), "");
    static_assert(offsetof(struct rbh_statx, stx_size)
               == offsetof(struct statx, stx_size), "");
    static_assert(offsetof(struct rbh_statx, stx_blocks)
               == offsetof(struct statx, stx_blocks), "");
    static_assert(offsetof(struct rbh_statx, stx_attributes_mask)
               == offsetof(struct statx, stx_attributes_mask), "");
    static_assert(offsetof(struct rbh_statx, stx_atime)
               == offsetof(struct statx, stx_atime), "");
    static_assert(offsetof(struct rbh_statx, stx_btime)
               == offsetof(struct statx, stx_btime), "");
    static_assert(offsetof(struct rbh_statx, stx_ctime)
               == offsetof(struct statx, stx_ctime), "");
    static_assert(offsetof(struct rbh_statx, stx_mtime)
               == offsetof(struct statx, stx_mtime), "");
    static_assert(offsetof(struct rbh_statx, stx_rdev_major)
               == offsetof(struct statx, stx_rdev_major), "");
    static_assert(offsetof(struct rbh_statx, stx_rdev_minor)
               == offsetof(struct statx, stx_rdev_minor), "");
    static_assert(offsetof(struct rbh_statx, stx_dev_major)
               == offsetof(struct statx, stx_dev_major), "");
    static_assert(offsetof(struct rbh_statx, stx_dev_minor)
               == offsetof(struct statx, stx_dev_minor), "");
#ifdef HAVE_STX_MNT_ID
    static_assert(offsetof(struct rbh_statx, stx_mnt_id)
               == offsetof(struct statx, stx_mnt_id), "");
#endif
}
END_TEST

START_TEST(rbh_statx_attr_compressed)
{
    static_assert(RBH_STATX_ATTR_COMPRESSED == STATX_ATTR_COMPRESSED, "");
}
END_TEST

START_TEST(rbh_statx_attr_immutable)
{
    static_assert(RBH_STATX_ATTR_IMMUTABLE == STATX_ATTR_IMMUTABLE, "");
}
END_TEST

START_TEST(rbh_statx_attr_append)
{
    static_assert(RBH_STATX_ATTR_APPEND == STATX_ATTR_APPEND, "");
}
END_TEST

START_TEST(rbh_statx_attr_nodump)
{
    static_assert(RBH_STATX_ATTR_NODUMP == STATX_ATTR_NODUMP, "");
}
END_TEST

START_TEST(rbh_statx_attr_encrypted)
{
    static_assert(RBH_STATX_ATTR_ENCRYPTED == STATX_ATTR_ENCRYPTED, "");
}
END_TEST

START_TEST(rbh_statx_attr_automount)
{
    static_assert(RBH_STATX_ATTR_AUTOMOUNT == STATX_ATTR_AUTOMOUNT, "");
}
END_TEST

START_TEST(rbh_statx_attr_mount_root)
{
#ifdef HAVE_STATX_ATTR_MOUNT_ROOT
    static_assert(RBH_STATX_ATTR_MOUNT_ROOT == STATX_ATTR_MOUNT_ROOT, "");
#endif
}
END_TEST

START_TEST(rbh_statx_attr_verity)
{
#ifdef HAVE_STATX_ATTR_VERITY
    static_assert(RBH_STATX_ATTR_VERITY == STATX_ATTR_VERITY, "");
#endif
}
END_TEST

START_TEST(rbh_statx_attr_dax)
{
#ifdef HAVE_STATX_ATTR_DAX
    static_assert(RBH_STATX_ATTR_DAX == STATX_ATTR_DAX, "");
#endif
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("statx linux compatibility");

    tests = tcase_create("statx flags");
    tcase_add_test(tests, type);
    tcase_add_test(tests, mode);
    tcase_add_test(tests, nlink);
    tcase_add_test(tests, uid);
    tcase_add_test(tests, gid);
    tcase_add_test(tests, atime);
    tcase_add_test(tests, mtime);
    tcase_add_test(tests, ctime);
    tcase_add_test(tests, ino);
    tcase_add_test(tests, size);
    tcase_add_test(tests, blocks);
    tcase_add_test(tests, btime);
    tcase_add_test(tests, rbh_atime);
    tcase_add_test(tests, rbh_btime);
    tcase_add_test(tests, rbh_ctime);
    tcase_add_test(tests, rbh_mtime);
    tcase_add_test(tests, rbh_rdev);
    tcase_add_test(tests, rbh_dev);
    tcase_add_test(tests, rbh_basic_stats);
    tcase_add_test(tests, rbh_all);

    suite_add_tcase(suite, tests);

    tests = tcase_create("struct rbh_statx");
    tcase_add_test(tests, rbh_statx_size);
    tcase_add_test(tests, rbh_statx_fields);

    suite_add_tcase(suite, tests);

    tests = tcase_create("statx_xattr flags");
    tcase_add_test(tests, rbh_statx_attr_compressed);
    tcase_add_test(tests, rbh_statx_attr_immutable);
    tcase_add_test(tests, rbh_statx_attr_append);
    tcase_add_test(tests, rbh_statx_attr_nodump);
    tcase_add_test(tests, rbh_statx_attr_encrypted);
    tcase_add_test(tests, rbh_statx_attr_automount);
    tcase_add_test(tests, rbh_statx_attr_mount_root);
    tcase_add_test(tests, rbh_statx_attr_verity);
    tcase_add_test(tests, rbh_statx_attr_dax);

    suite_add_tcase(suite, tests);

    return suite;
}

int
main(void)
{
    int number_failed;
    Suite *suite;
    SRunner *runner;

    suite = unit_suite();
    runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
