/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "robinhood/list.h"

#include "check-compat.h"

struct test_list_elem {
    int value;
    struct rbh_list_node link;
};

/*----------------------------------------------------------------------------*
 |                                 unit tests                                 |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                          rbh_list_init                             |
     *--------------------------------------------------------------------*/

START_TEST(rli_new_list_is_empty)
{
    struct rbh_list_node list;

    rbh_list_init(&list);

    ck_assert_int_eq(rbh_list_empty(&list), 1);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                          rbh_list_add                              |
     *--------------------------------------------------------------------*/

START_TEST(rla_add_one)
{
    struct test_list_elem *node;
    struct test_list_elem elem;
    struct rbh_list_node list;

    rbh_list_init(&list);

    elem.value = 1;
    rbh_list_add(&list, &elem.link);

    node = rbh_list_first(&list, struct test_list_elem, link);
    ck_assert_ptr_eq(node, &elem);
    ck_assert_int_eq(node->value, 1);
}
END_TEST

START_TEST(rla_add_many)
{
    struct test_list_elem elems[5];
    struct test_list_elem *node;
    struct rbh_list_node list;

    rbh_list_init(&list);

    for (int i = 0; i < 5; i++) {
        elems[i].value = i + 1;
        rbh_list_add(&list, &elems[i].link);
    }

    node = rbh_list_first(&list, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 5);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 4);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 3);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 2);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 1);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                          rbh_list_add_tail                         |
     *--------------------------------------------------------------------*/

START_TEST(rlat_add_one)
{
    struct test_list_elem *node;
    struct test_list_elem elem;
    struct rbh_list_node list;

    rbh_list_init(&list);

    elem.value = 1;
    rbh_list_add_tail(&list, &elem.link);

    node = rbh_list_first(&list, struct test_list_elem, link);
    ck_assert_ptr_eq(node, &elem);
    ck_assert_int_eq(node->value, 1);
}
END_TEST

START_TEST(rlat_add_many)
{
    struct test_list_elem elems[5];
    struct test_list_elem *node;
    struct rbh_list_node list;

    rbh_list_init(&list);

    for (int i = 0; i < 5; i++) {
        elems[i].value = i + 1;
        rbh_list_add_tail(&list, &elems[i].link);
    }

    node = rbh_list_first(&list, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 1);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 2);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 3);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 4);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 5);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                          rbh_list_splice_tail                      |
     *--------------------------------------------------------------------*/

START_TEST(rlst_two_lists)
{
    struct test_list_elem elems[4];
    struct test_list_elem *node;
    struct rbh_list_node list1;
    struct rbh_list_node list2;

    rbh_list_init(&list1);
    rbh_list_init(&list2);

    for (int i = 0; i < 4; i++)
        elems[i].value = i + 1;

    rbh_list_add(&list1, &elems[0].link);
    rbh_list_add(&list1, &elems[1].link);
    rbh_list_add(&list2, &elems[2].link);
    rbh_list_add(&list2, &elems[3].link);

    // list1: 2->1, list2: 4->3
    rbh_list_splice_tail(&list1, &list2);

    node = rbh_list_first(&list1, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 2);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 1);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 4);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 3);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                          rbh_list_del                              |
     *--------------------------------------------------------------------*/

START_TEST(rld_entries)
{
    struct test_list_elem elems[3];
    struct test_list_elem *node;
    struct rbh_list_node list;

    rbh_list_init(&list);

    for (int i = 0; i < 3; i++) {
        elems[i].value = i + 1;
        rbh_list_add(&list, &elems[i].link);
    }

    node = rbh_list_first(&list, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 3);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 2);

    rbh_list_del(&node->link);

    node = rbh_list_first(&list, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 3);

    node = rbh_list_next(&node->link, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 1);

    rbh_list_del(&node->link);

    node = rbh_list_first(&list, struct test_list_elem, link);
    ck_assert_int_eq(node->value, 3);

    rbh_list_del(&node->link);
    ck_assert_int_eq(rbh_list_empty(&list), 1);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                          rbh_list_foreach                          |
     *--------------------------------------------------------------------*/

START_TEST(rlfe_empty)
{
    struct test_list_elem *node;
    struct rbh_list_node list;

    rbh_list_init(&list);

    rbh_list_foreach(&list, node, link)
        ck_abort_msg("We should not enter the loop on an empty list");
}
END_TEST

START_TEST(rlfe_iteration)
{
    struct test_list_elem elems[5] = {0};
    struct test_list_elem *node;
    struct rbh_list_node list;
    int i = 1;

    rbh_list_init(&list);

    for (int i = 0; i < 5; i++) {
        elems[i].value = i + 1;
        rbh_list_add_tail(&list, &elems[i].link);
    }

    rbh_list_foreach(&list, node, link)
        ck_assert_int_eq(node->value, i++);
}
END_TEST

    /*--------------------------------------------------------------------*
     |                          rbh_list_foreach_safe                     |
     *--------------------------------------------------------------------*/

START_TEST(rlfes_iteration)
{
    struct test_list_elem elems[5] = {0};
    struct test_list_elem *node, *elem;
    struct rbh_list_node list;
    int i = 1;

    rbh_list_init(&list);

    for (int i = 0; i < 5; i++) {
        elems[i].value = i + 1;
        rbh_list_add_tail(&list, &elems[i].link);
    }

    rbh_list_foreach_safe(&list, node, elem, link)
        ck_assert_int_eq(node->value, i++);
}
END_TEST

START_TEST(rlfes_del_while_iter)
{
    struct test_list_elem elems[5] = {0};
    struct test_list_elem *node, *elem;
    struct rbh_list_node list;
    int i = 1;

    rbh_list_init(&list);

    for (int i = 0; i < 5; i++) {
        elems[i].value = i + 1;
        rbh_list_add_tail(&list, &elems[i].link);
    }

    rbh_list_foreach_safe(&list, node, elem, link) {
        rbh_list_del(&node->link);
        ck_assert_int_eq(node->value, i++);
    }
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("unit tests");

    tests = tcase_create("rbh_list_init");
    tcase_add_test(tests, rli_new_list_is_empty);

    tests = tcase_create("rbh_list_add");
    tcase_add_test(tests, rla_add_one);
    tcase_add_test(tests, rla_add_many);
    tcase_add_test(tests, rlat_add_one);
    tcase_add_test(tests, rlat_add_many);
    tcase_add_test(tests, rlst_two_lists);
    tcase_add_test(tests, rld_entries);
    tcase_add_test(tests, rlfe_empty);
    tcase_add_test(tests, rlfe_iteration);
    tcase_add_test(tests, rlfes_iteration);
    tcase_add_test(tests, rlfes_del_while_iter);

    suite_add_tcase(suite, tests);

    return suite;
}

int
main(void)
{
    int number_failed;
    SRunner *runner;

    runner = srunner_create(unit_suite());

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
