/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "check-compat.h"
#include "robinhood/uri.h"

#define ck_assert_raw_uri_eq(X, Y) do { \
    ck_assert_str_eq((X)->scheme, (Y)->scheme); \
    ck_assert_pstr_eq((X)->userinfo, (Y)->userinfo); \
    ck_assert_pstr_eq((X)->host, (Y)->host); \
    ck_assert_pstr_eq((X)->port, (Y)->port); \
    ck_assert_str_eq((X)->path, (Y)->path); \
    ck_assert_pstr_eq((X)->query, (Y)->query); \
    ck_assert_pstr_eq((X)->fragment, (Y)->fragment); \
} while (false)

#define ck_assert_id_eq(X, Y) do { \
    ck_assert_uint_eq((X)->size, (Y)->size); \
    if ((X)->size != 0) \
        ck_assert_mem_eq((X)->data, (Y)->data, (X)->size); \
} while (false)

#define ck_assert_uri_eq(X, Y) do { \
    ck_assert_str_eq((X)->backend, (Y)->backend); \
    ck_assert_str_eq((X)->fsname, (Y)->fsname); \
    ck_assert_id_eq(&(X)->id, &(Y)->id); \
} while (false)

/*----------------------------------------------------------------------------*
 |                            rbh_parse_raw_uri()                             |
 *----------------------------------------------------------------------------*/

START_TEST(rpru_empty)
{
    struct rbh_raw_uri raw_uri;
    char string[] = "";

    errno = 0;
    ck_assert_int_eq(rbh_parse_raw_uri(&raw_uri, string), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rpru_scheme)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "",
    };
    struct rbh_raw_uri raw_uri;
    char string[] = RBH_SCHEME ":";

    ck_assert_int_eq(rbh_parse_raw_uri(&raw_uri, string), 0);
    ck_assert_raw_uri_eq(&raw_uri, &RAW_URI);
}
END_TEST

START_TEST(rpru_missing_scheme)
{
    struct rbh_raw_uri raw_uri;
    char string[] = "a";

    errno = 0;
    ck_assert_int_eq(rbh_parse_raw_uri(&raw_uri, string), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rpru_fragment)
{
#define FRAGMENT "test"
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "",
        .fragment = FRAGMENT,
    };
    struct rbh_raw_uri raw_uri;
    char string[] = RBH_SCHEME ":#" FRAGMENT;

    ck_assert_int_eq(rbh_parse_raw_uri(&raw_uri, string), 0);
    ck_assert_raw_uri_eq(&raw_uri, &RAW_URI);
#undef FRAGMENT
}
END_TEST

START_TEST(rpru_query)
{
#define QUERY "query"
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "",
        .query = QUERY,
    };
    struct rbh_raw_uri raw_uri;
    char string[] = RBH_SCHEME ":?" QUERY;

    ck_assert_int_eq(rbh_parse_raw_uri(&raw_uri, string), 0);
    ck_assert_raw_uri_eq(&raw_uri, &RAW_URI);
#undef QUERY
}
END_TEST

START_TEST(rpru_no_athority_absolute_path)
{
#define PATH "/path"
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = PATH,
    };
    struct rbh_raw_uri raw_uri;
    char string[] = RBH_SCHEME ":" PATH;

    ck_assert_int_eq(rbh_parse_raw_uri(&raw_uri, string), 0);
    ck_assert_raw_uri_eq(&raw_uri, &RAW_URI);
#undef PATH
}
END_TEST

START_TEST(rpru_no_athority_relative_path)
{
#define PATH "path"
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = PATH,
    };
    struct rbh_raw_uri raw_uri;
    char string[] = RBH_SCHEME ":" PATH;

    ck_assert_int_eq(rbh_parse_raw_uri(&raw_uri, string), 0);
    ck_assert_raw_uri_eq(&raw_uri, &RAW_URI);
#undef PATH
}
END_TEST

START_TEST(rpru_empty_authority_empty_path)
{
#define HOST "host"
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "",
    };
    struct rbh_raw_uri raw_uri;
    char string[] = RBH_SCHEME "://";

    ck_assert_int_eq(rbh_parse_raw_uri(&raw_uri, string), 0);
    ck_assert_raw_uri_eq(&raw_uri, &RAW_URI);
#undef HOST
}
END_TEST

START_TEST(rpru_empty_authority_path)
{
#define PATH "/path"
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = PATH,
    };
    struct rbh_raw_uri raw_uri;
    char string[] = RBH_SCHEME "://" PATH;

    ck_assert_int_eq(rbh_parse_raw_uri(&raw_uri, string), 0);
    ck_assert_raw_uri_eq(&raw_uri, &RAW_URI);
#undef PATH
}
END_TEST

START_TEST(rpru_userinfo)
{
#define USERINFO "userinfo"
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .userinfo = USERINFO,
        .path = "",
    };
    struct rbh_raw_uri raw_uri;
    char string[] = RBH_SCHEME "://" USERINFO "@";

    ck_assert_int_eq(rbh_parse_raw_uri(&raw_uri, string), 0);
    ck_assert_raw_uri_eq(&raw_uri, &RAW_URI);
#undef USERINFO
}
END_TEST

START_TEST(rpru_host)
{
#define HOST "host"
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .host = HOST,
        .path = "",
    };
    struct rbh_raw_uri raw_uri;
    char string[] = RBH_SCHEME "://" HOST;

    ck_assert_int_eq(rbh_parse_raw_uri(&raw_uri, string), 0);
    ck_assert_raw_uri_eq(&raw_uri, &RAW_URI);
#undef HOST
}
END_TEST

START_TEST(rpru_port)
{
#define PORT "12345"
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .port = PORT,
        .path = "",
    };
    struct rbh_raw_uri raw_uri;
    char string[] = RBH_SCHEME "://:" PORT;

    ck_assert_int_eq(rbh_parse_raw_uri(&raw_uri, string), 0);
    ck_assert_raw_uri_eq(&raw_uri, &RAW_URI);
#undef PORT
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_percent_decode()                            |
 *----------------------------------------------------------------------------*/

START_TEST(rpd_every_hexa_char)
{
    const char DECODED[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                            15, '\0'};
    char encoded[] = "%00%01%02%03%04%05%06%07%08%09%0a%0b%0c%0d%0e%0f";

    ck_assert_uint_eq(rbh_percent_decode(encoded), sizeof(DECODED) - 1);
    ck_assert_str_eq(encoded, DECODED);
}
END_TEST

START_TEST(rpd_fully_encoded)
{
    const char DECODED[] = "Hello World";
    char encoded[(sizeof(DECODED) - 1) * 3 + 1];

    for (size_t i = 0; i < sizeof(DECODED) - 1; i++)
        sprintf(encoded + i * 3, "%%%.2x", DECODED[i]);
    encoded[sizeof(encoded) - 1] = '\0';

    ck_assert_uint_eq(rbh_percent_decode(encoded), sizeof(DECODED) - 1);
    ck_assert_str_eq(encoded, DECODED);
}
END_TEST

START_TEST(rpd_unencoded)
{
#define STRING "Hello World"
    char unencoded[] = STRING;

    ck_assert_uint_eq(rbh_percent_decode(unencoded), 0);
    ck_assert_str_eq(unencoded, STRING);
#undef STRING
}
END_TEST

START_TEST(rpd_too_short)
{
    char encoded[] = "%e";

    errno = 0;
    ck_assert_int_eq(rbh_percent_decode(encoded), -1);
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rpd_not_hexa)
{
    char encoded[] = "%0g";

    errno = 0;
    ck_assert_int_eq(rbh_percent_decode(encoded), -1);
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                              rbh_parse_uri()                               |
 *----------------------------------------------------------------------------*/

START_TEST(rpu_missing_scheme)
{
    struct rbh_raw_uri raw_uri = {
        .scheme = NULL,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rpu_wrong_scheme)
{
    struct rbh_raw_uri raw_uri = {
        .scheme = "",
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rpu_no_colon)
{
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = "",
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rpu_encoding_error_not_hexa)
{
    char path[] = "%g0:";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rpu_encoding_error_short_code)
{
    char path[] = ":%0";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rpu_decode_basic)
{
    char path[] = "%00:";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
    };
    const struct rbh_uri URI = {
        .backend = "",
        .fsname = "",
        .id = {
            .size = 0,
        },
    };
    struct rbh_uri uri;

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_uri_eq(&uri, &URI);
}
END_TEST

START_TEST(rpu_decode_lowercase)
{
    const char BACKEND[] = { 0x0a, '\0' };
    char path[] = "%0a:";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
    };
    struct rbh_uri uri;

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_str_eq(uri.backend, BACKEND);
}
END_TEST

START_TEST(rpu_decode_uppercase)
{
    const char BACKEND[] = { 0x0a, '\0' };
    char path[] = "%0A:";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
    };
    struct rbh_uri uri;

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_str_eq(uri.backend, BACKEND);
}
END_TEST

START_TEST(rpu_empty_fragment)
{
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
    };
    struct rbh_uri uri;

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_uint_eq(uri.id.size, 0);
}
END_TEST

START_TEST(rpu_empty_id_fragment)
{
    char fragment[] = "[]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_uint_eq(uri.id.size, 0);
}
END_TEST

START_TEST(rpu_misencoded_id_fragment)
{
    char fragment[] = "[%]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rpu_id_fragment_missing_opening_bracket)
{
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = "0",
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rpu_id_fragment_missing_closing_bracket)
{
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = "[",
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rpu_id_fragment)
{
    const char DATA[] = { 0, '1', 2, '3', 4, '5', 6, '7' };
    char path[] = ":";
    char fragment[] = "[%001%023%045%067]";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_uint_eq(uri.id.size, sizeof(DATA));
    ck_assert_mem_eq(uri.id.data, DATA, sizeof(DATA));
}
END_TEST

static size_t
fid_copy(char *data, size_t size, uint64_t sequence, uint32_t oid,
         uint32_t version)
{
    ck_assert_uint_ge(size, sizeof(uint64_t) + sizeof(uint32_t) * 2);

    memcpy(data, &sequence, sizeof(sequence));
    data += sizeof(sequence);
    memcpy(data, &oid, sizeof(oid));
    data += sizeof(oid);
    memcpy(data, &version, sizeof(version));
    data += sizeof(version);

    return sizeof(sequence) + sizeof(oid) + sizeof(version);
}

/* Build a lustre ID and copy it into a buffer */
static size_t
lustre_id_copy(char *data, size_t size, uint64_t sequence, uint32_t oid,
               uint32_t version)
{
    const int FILEID_LUSTRE = 0x97;
    size_t used = 0;

    ck_assert_uint_ge(size, sizeof(FILEID_LUSTRE));
    memcpy(data, &FILEID_LUSTRE, sizeof(FILEID_LUSTRE));
    used += sizeof(FILEID_LUSTRE);
    used += fid_copy(data + used, size - used, sequence, oid, version);
    used += fid_copy(data + used, size - used, 0, 0, 0);

    return used;
}

START_TEST(rpu_fid_fragment)
{
    char data[MAX_HANDLE_SZ];
    char fragment[] = "[0x0:0x1:0x2]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;
    size_t size;

    size = lustre_id_copy(data, sizeof(data), 0, 1, 2);

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_uint_eq(uri.id.size, size);
    ck_assert_mem_eq(uri.id.data, data, size);
}
END_TEST

START_TEST(rpu_id_single_unencoded_colon_fragment)
{
    char fragment[] = "[:]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rpu_id_single_encoded_colon_fragment)
{
    char fragment[] = "[%3a]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    const struct rbh_uri URI = {
        .backend = "",
        .fsname = "",
        .id = {
            .data = ":",
            .size = 1,
        },
    };
    struct rbh_uri uri;

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_uri_eq(&uri, &URI);
}
END_TEST

START_TEST(rpu_id_two_unencoded_colons_fragment)
{
    char data[MAX_HANDLE_SZ];
    char fragment[] = "[::]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;
    size_t size;

    size = lustre_id_copy(data, sizeof(data), 0, 0, 0);

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_uint_eq(uri.id.size, size);
    ck_assert_mem_eq(uri.id.data, data, size);
}
END_TEST

START_TEST(rpu_fid_sequence_is_not_hexa_fragment)
{
    char fragment[] = "[0x0g:0x1:0x2]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rpu_fid_oid_is_not_hexa_fragment)
{
    char fragment[] = "[0x0:0x1g:0x2]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rpu_fid_version_is_not_hexa_fragment)
{
    char fragment[] = "[0x0:0x1:0x2g]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rpu_fid_encoded_fragment)
{
    char data[MAX_HANDLE_SZ];
    char fragment[] = "[%30%78%30:0x1:%30%78%32]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;
    size_t size;

    size = lustre_id_copy(data, sizeof(data), 0, 1, 2);

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_uint_eq(uri.id.size, size);
    ck_assert_mem_eq(uri.id.data, data, size);
}
END_TEST

START_TEST(rpu_fid_misencoded_sequence_fragment)
{
    char fragment[] = "[%3g::]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rpu_fid_misencoded_oid_fragment)
{
    char fragment[] = "[0x0:%3g:]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rpu_fid_misencoded_version_fragment)
{
    char fragment[] = "[0x0:0x1:%3g]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rpu_fid_max_sequence_fragment)
{
    char data[MAX_HANDLE_SZ];
    char fragment[] = "[0xffffffffffffffff:0x0:0x0]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;
    size_t size;

    size = lustre_id_copy(data, sizeof(data), UINT64_MAX, 0, 0);

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_uint_eq(uri.id.size, size);
    ck_assert_mem_eq(uri.id.data, data, size);
}
END_TEST

START_TEST(rpu_fid_overflowed_sequence_fragment)
{
    char fragment[] = "[0x10000000000000000:0x0:0x0]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, ERANGE);
}
END_TEST

START_TEST(rpu_fid_max_oid_fragment)
{
    char data[MAX_HANDLE_SZ];
    char fragment[] = "[0x0:0xffffffff:0x0]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;
    size_t size;

    size = lustre_id_copy(data, sizeof(data), 0, UINT32_MAX, 0);

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_uint_eq(uri.id.size, size);
    ck_assert_mem_eq(uri.id.data, data, size);
}
END_TEST

START_TEST(rpu_fid_overflowed_oid_fragment)
{
    char fragment[] = "[0x0:0x100000000:0x0]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, ERANGE);
}
END_TEST

START_TEST(rpu_fid_max_version_fragment)
{
    char data[MAX_HANDLE_SZ];
    char fragment[] = "[0x0:0x0:0xffffffff]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;
    size_t size;

    size = lustre_id_copy(data, sizeof(data), 0, 0, UINT32_MAX);

    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), 0);
    ck_assert_uint_eq(uri.id.size, size);
    ck_assert_mem_eq(uri.id.data, data, size);
}
END_TEST

START_TEST(rpu_fid_overflowed_version_fragment)
{
    char fragment[] = "[0x0:0x0:0x100000000]";
    char path[] = ":";
    struct rbh_raw_uri raw_uri = {
        .scheme = RBH_SCHEME,
        .path = path,
        .fragment = fragment,
    };
    struct rbh_uri uri;

    errno = 0;
    ck_assert_int_eq(rbh_parse_uri(&uri, &raw_uri), -1);
    ck_assert_int_eq(errno, ERANGE);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("uri");
    tests = tcase_create("rbh_parse_raw_uri()");
    tcase_add_test(tests, rpru_empty);
    tcase_add_test(tests, rpru_scheme);
    tcase_add_test(tests, rpru_missing_scheme);
    tcase_add_test(tests, rpru_fragment);
    tcase_add_test(tests, rpru_query);
    tcase_add_test(tests, rpru_no_athority_absolute_path);
    tcase_add_test(tests, rpru_no_athority_relative_path);
    tcase_add_test(tests, rpru_empty_authority_empty_path);
    tcase_add_test(tests, rpru_empty_authority_path);
    tcase_add_test(tests, rpru_userinfo);
    tcase_add_test(tests, rpru_host);
    tcase_add_test(tests, rpru_port);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_percent_decode()");
    tcase_add_test(tests, rpd_every_hexa_char);
    tcase_add_test(tests, rpd_fully_encoded);
    tcase_add_test(tests, rpd_unencoded);
    tcase_add_test(tests, rpd_too_short);
    tcase_add_test(tests, rpd_not_hexa);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_parse_uri()");
    tcase_add_test(tests, rpu_missing_scheme);
    tcase_add_test(tests, rpu_wrong_scheme);
    tcase_add_test(tests, rpu_no_colon);
    tcase_add_test(tests, rpu_encoding_error_not_hexa);
    tcase_add_test(tests, rpu_encoding_error_short_code);
    tcase_add_test(tests, rpu_decode_basic);
    tcase_add_test(tests, rpu_decode_lowercase);
    tcase_add_test(tests, rpu_decode_uppercase);
    tcase_add_test(tests, rpu_empty_fragment);
    tcase_add_test(tests, rpu_empty_id_fragment);
    tcase_add_test(tests, rpu_misencoded_id_fragment);
    tcase_add_test(tests, rpu_id_fragment_missing_opening_bracket);
    tcase_add_test(tests, rpu_id_fragment_missing_closing_bracket);
    tcase_add_test(tests, rpu_id_fragment);
    tcase_add_test(tests, rpu_fid_fragment);
    tcase_add_test(tests, rpu_id_single_unencoded_colon_fragment);
    tcase_add_test(tests, rpu_id_single_encoded_colon_fragment);
    tcase_add_test(tests, rpu_id_two_unencoded_colons_fragment);
    tcase_add_test(tests, rpu_fid_sequence_is_not_hexa_fragment);
    tcase_add_test(tests, rpu_fid_oid_is_not_hexa_fragment);
    tcase_add_test(tests, rpu_fid_version_is_not_hexa_fragment);
    tcase_add_test(tests, rpu_fid_encoded_fragment);
    tcase_add_test(tests, rpu_fid_misencoded_sequence_fragment);
    tcase_add_test(tests, rpu_fid_misencoded_oid_fragment);
    tcase_add_test(tests, rpu_fid_misencoded_version_fragment);
    tcase_add_test(tests, rpu_fid_max_sequence_fragment);
    tcase_add_test(tests, rpu_fid_overflowed_sequence_fragment);
    tcase_add_test(tests, rpu_fid_max_oid_fragment);
    tcase_add_test(tests, rpu_fid_overflowed_oid_fragment);
    tcase_add_test(tests, rpu_fid_max_version_fragment);
    tcase_add_test(tests, rpu_fid_overflowed_version_fragment);

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
