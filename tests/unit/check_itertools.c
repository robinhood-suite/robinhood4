/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>

#include "check-compat.h"
#include "robinhood/itertools.h"

/*----------------------------------------------------------------------------*
 |                            rbh_array_iterator()                            |
 *----------------------------------------------------------------------------*/

START_TEST(rai_basic)
{
    static const char STRING[] = "abcdefghijklmno";
    struct rbh_iterator *iterator;

    iterator = rbh_array_iterator(STRING, sizeof(*STRING), sizeof(STRING));
    ck_assert_ptr_nonnull(iterator);

    for (size_t i = 0; i < sizeof(STRING); i++)
        ck_assert_mem_eq(rbh_iter_next(iterator), &STRING[i], sizeof(*STRING));

    errno = 0;
    ck_assert_ptr_null(rbh_iter_next(iterator));
    ck_assert_int_eq(errno, ENODATA);

    rbh_iter_destroy(iterator);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("itertools");
    tests = tcase_create("rbh_array_iterator()");
    tcase_add_test(tests, rai_basic);

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
