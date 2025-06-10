/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
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
#include "check_macros.h"
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

#define ck_assert_uri_authority_eq(X, Y) do { \
    ck_assert_str_eq((X)->username, (Y)->username); \
    ck_assert_str_eq((X)->password, (Y)->password); \
    ck_assert_str_eq((X)->host, (Y)->host); \
    ck_assert_uint_eq((X)->port, (Y)->port); \
} while (false)

#define ck_assert_uri_eq(X, Y) do { \
    ck_assert_int_eq((X)->type, (Y)->type); \
    ck_assert_str_eq((X)->backend, (Y)->backend); \
    ck_assert_str_eq((X)->fsname, (Y)->fsname); \
    if ((X)->authority) { \
        ck_assert_ptr_nonnull((Y)->authority); \
        ck_assert_uri_authority_eq((X)->authority, (Y)->authority); \
    } else  { \
        ck_assert_ptr_null((Y)->authority); \
    } \
    switch ((X)->type) { \
    case RBH_UT_BARE: \
        break; \
    case RBH_UT_ID: \
        ck_assert_id_eq((X)->id, (Y)->id); \
        break; \
    case RBH_UT_PATH: \
        ck_assert_str_eq((X)->path, (Y)->path); \
        break; \
    } \
} while (false)

#include "lu_fid.h"

/*----------------------------------------------------------------------------*
 |                         rbh_raw_uri_from_string()                          |
 *----------------------------------------------------------------------------*/

START_TEST(rrufs_empty)
{
    const char STRING[] = "";

    errno = 0;
    ck_assert_ptr_null(rbh_raw_uri_from_string(STRING));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rrufs_scheme)
{
    const char STRING[] = RBH_SCHEME ":";
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "",
    };
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(STRING);
    ck_assert_ptr_nonnull(raw_uri);
    ck_assert_raw_uri_eq(raw_uri, &RAW_URI);

    free(raw_uri);
}
END_TEST

START_TEST(rrufs_missing_scheme)
{
    const char STRING[] = "a";

    errno = 0;
    ck_assert_ptr_null(rbh_raw_uri_from_string(STRING));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rrufs_empty_fragment)
{
    const char STRING[] = RBH_SCHEME ":#";
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "",
        .fragment = "",
    };
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(STRING);
    ck_assert_ptr_nonnull(raw_uri);
    ck_assert_raw_uri_eq(raw_uri, &RAW_URI);

    free(raw_uri);
}
END_TEST

START_TEST(rrufs_fragment)
{
    const char STRING[] = RBH_SCHEME ":#test";
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "",
        .fragment = "test",
    };
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(STRING);
    ck_assert_ptr_nonnull(raw_uri);
    ck_assert_raw_uri_eq(raw_uri, &RAW_URI);

    free(raw_uri);
}
END_TEST

START_TEST(rrufs_query)
{
    const char STRING[] = RBH_SCHEME ":?query";
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "",
        .query = "query",
    };
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(STRING);
    ck_assert_ptr_nonnull(raw_uri);
    ck_assert_raw_uri_eq(raw_uri, &RAW_URI);

    free(raw_uri);
}
END_TEST

START_TEST(rrufs_no_athority_absolute_path)
{
    const char STRING[] = RBH_SCHEME ":/absolute/path";
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "/absolute/path",
    };
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(STRING);
    ck_assert_ptr_nonnull(raw_uri);
    ck_assert_raw_uri_eq(raw_uri, &RAW_URI);

    free(raw_uri);
}
END_TEST

START_TEST(rrufs_no_athority_relative_path)
{
    const char STRING[] = RBH_SCHEME ":relative/path";
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "relative/path",
    };
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(STRING);
    ck_assert_ptr_nonnull(raw_uri);
    ck_assert_raw_uri_eq(raw_uri, &RAW_URI);

    free(raw_uri);
}
END_TEST

START_TEST(rrufs_empty_authority_empty_path)
{
    const char STRING[] = RBH_SCHEME "://";
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .host = "",
        .path = "",
    };
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(STRING);
    ck_assert_ptr_nonnull(raw_uri);
    ck_assert_raw_uri_eq(raw_uri, &RAW_URI);

    free(raw_uri);
}
END_TEST

START_TEST(rrufs_empty_authority_path)
{
    const char STRING[] = RBH_SCHEME ":///path";
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .host = "",
        .path = "/path",
    };
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(STRING);
    ck_assert_ptr_nonnull(raw_uri);
    ck_assert_raw_uri_eq(raw_uri, &RAW_URI);

    free(raw_uri);
}
END_TEST

START_TEST(rrufs_userinfo)
{
    const char STRING[] = RBH_SCHEME "://userinfo@";
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .userinfo = "userinfo",
        .host = "",
        .path = "",
    };
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(STRING);
    ck_assert_ptr_nonnull(raw_uri);
    ck_assert_raw_uri_eq(raw_uri, &RAW_URI);

    free(raw_uri);
}
END_TEST

START_TEST(rrufs_host)
{
    const char STRING[] = RBH_SCHEME "://host";
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .host = "host",
        .path = "",
    };
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(STRING);
    ck_assert_ptr_nonnull(raw_uri);
    ck_assert_raw_uri_eq(raw_uri, &RAW_URI);

    free(raw_uri);
}
END_TEST

START_TEST(rrufs_port)
{
    const char STRING[] = RBH_SCHEME "://:12345";
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .host = "",
        .port = "12345",
        .path = "",
    };
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(STRING);
    ck_assert_ptr_nonnull(raw_uri);
    ck_assert_raw_uri_eq(raw_uri, &RAW_URI);

    free(raw_uri);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_percent_decode()                            |
 *----------------------------------------------------------------------------*/

START_TEST(rpd_every_hexa_char)
{
    const char DECODED[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                            15};
    char encoded[] = "%00%01%02%03%04%05%06%07%08%09%0a%0b%0c%0d%0e%0f";

    ck_assert_int_eq(rbh_percent_decode(encoded, encoded, sizeof(encoded)),
                     sizeof(DECODED));
    ck_assert_mem_eq(encoded, DECODED, sizeof(DECODED));
}
END_TEST

START_TEST(rpd_fully_encoded)
{
    const char DECODED[] = "Hello World";
    char encoded[sizeof(DECODED) * 3 + 1];

    for (size_t i = 0; i < sizeof(DECODED); i++)
        sprintf(encoded + i * 3, "%%%.2x", DECODED[i]);
    encoded[sizeof(encoded) - 1] = '\0';

    ck_assert_int_eq(rbh_percent_decode(encoded, encoded, sizeof(encoded)),
                     sizeof(DECODED));
    ck_assert_str_eq(encoded, DECODED);
}
END_TEST

START_TEST(rpd_unencoded)
{
    const char UNENCODED[] = "Hello World";
    char decoded[sizeof(UNENCODED)];
    size_t n = sizeof(UNENCODED);

    ck_assert_int_eq(rbh_percent_decode(decoded, UNENCODED, n), n - 1);
    /*                                                            ^^^
     *                              The terminating null byte is not copied
     *                                     vvv
     */
    ck_assert_mem_eq(decoded, UNENCODED, n - 1);
}
END_TEST

START_TEST(rpd_too_short)
{
    char misencoded[] = "%e";

    errno = 0;
    ck_assert_int_eq(
            rbh_percent_decode(misencoded, misencoded, sizeof(misencoded)),
            -1
            );
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rpd_not_hexa_first)
{
    char misencoded[] = "%g0";

    errno = 0;
    ck_assert_int_eq(
            rbh_percent_decode(misencoded, misencoded, sizeof(misencoded)), -1
            );
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rpd_not_hexa_second)
{
    char misencoded[] = "%0g";

    errno = 0;
    ck_assert_int_eq(
            rbh_percent_decode(misencoded, misencoded, sizeof(misencoded)), -1
            );
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rpd_case_insensitive)
{
    const char LOWERCASE[] = "abcdef";
    const char UPPERCASE[] = "ABCDEF";
    const char DECODED[] = {0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
    char lowercode[4];
    char uppercode[4];

    for (size_t i = 0; i < sizeof(LOWERCASE) - 1; i++) {
        sprintf(lowercode, "%%0%c", LOWERCASE[i]);
        sprintf(uppercode, "%%0%c", UPPERCASE[i]);
        ck_assert_int_eq(rbh_percent_decode(lowercode, lowercode, 4), 1);
        ck_assert_int_eq(rbh_percent_decode(uppercode, uppercode, 4), 1);

        ck_assert_int_eq(*lowercode, DECODED[i]);
        ck_assert_int_eq(*uppercode, DECODED[i]);
    }
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           rbh_uri_from_raw_uri()                           |
 *----------------------------------------------------------------------------*/

START_TEST(rufru_wrong_scheme)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = "",
    };

    errno = 0;
    ck_assert_ptr_null(rbh_uri_from_raw_uri(&RAW_URI));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rufru_no_colon)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "",
    };

    errno = 0;
    ck_assert_ptr_null(rbh_uri_from_raw_uri(&RAW_URI));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rufru_encoded_backend_and_fsname)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "%00:%00",
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_BARE,
        .backend = "",
        .fsname = "",
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(uri, &URI);

    free(uri);
}
END_TEST

START_TEST(rufru_misencoded_backend)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = "%:",
    };

    errno = 0;
    ck_assert_ptr_null(rbh_uri_from_raw_uri(&RAW_URI));
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rufru_misencoded_fsname)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":%",
    };

    errno = 0;
    ck_assert_ptr_null(rbh_uri_from_raw_uri(&RAW_URI));
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rufru_no_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_BARE,
        .backend = "",
        .fsname = "",
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(uri, &URI);

    free(uri);
}
END_TEST

START_TEST(rufru_empty_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "",
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_PATH,
        .backend = "",
        .fsname = "",
        .path = "",
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(&URI, uri);

    free(uri);
}
END_TEST

START_TEST(rufru_empty_id_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "[]",
    };
    const struct rbh_id ID = {
        .size = 0,
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_ID,
        .backend = "",
        .fsname = "",
        .id = &ID,
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(uri, &URI);

    free(uri);
}
END_TEST

START_TEST(rufru_misencoded_id_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "[%]",
    };

    errno = 0;
    ck_assert_ptr_null(rbh_uri_from_raw_uri(&RAW_URI));
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rufru_misencoded_path_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "%",
    };

    errno = 0;
    ck_assert_ptr_null(rbh_uri_from_raw_uri(&RAW_URI));
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

START_TEST(rufru_path_fragment_with_bracket)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "[",
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_PATH,
        .backend = "",
        .fsname = "",
        .path = "[",
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(&URI, uri);

    free(uri);
}
END_TEST

START_TEST(rufru_id_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "[%001%023%045%067]",
    };
    const char DATA[] = { 0x00, '1', 0x02, '3', 0x04, '5', 0x06, '7' };
    const struct rbh_id ID = {
        .data = DATA,
        .size = sizeof(DATA),
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_ID,
        .backend = "",
        .fsname = "",
        .id = &ID,
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(uri, &URI);

    free(uri);
}
END_TEST

START_TEST(rufru_fid_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "[0x0:0x1:0x2]",
    };
    const struct lu_fid fid = {0, 1, 2};
    struct rbh_id *id = rbh_id_from_lu_fid(&fid);
    const struct rbh_uri URI = {
        .type = RBH_UT_ID,
        .backend = "",
        .fsname = "",
        .id = id,
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(uri, &URI);

    free(uri);
    free(id);
}
END_TEST

START_TEST(rufru_bad_fid_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "[0xg::]",
    };

    errno = 0;
    ck_assert_ptr_null(rbh_uri_from_raw_uri(&RAW_URI));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rufru_fid_and_garbage_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "[::abc]",
    };

    errno = 0;
    ck_assert_ptr_null(rbh_uri_from_raw_uri(&RAW_URI));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rufru_misencoded_fid_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "[%::]",
    };

    errno = 0;
    ck_assert_ptr_null(rbh_uri_from_raw_uri(&RAW_URI));
    ck_assert_int_eq(errno, EILSEQ);
}
END_TEST

/* A single unencoded colon is not enough to classify the ID as a FID */
START_TEST(rufru_id_single_unencoded_colon_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "[:]",
    };
    const struct rbh_id ID = {
        .data = ":",
        .size = 1,
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_ID,
        .backend = "",
        .fsname = "",
        .id = &ID,
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(uri, &URI);

    free(uri);
}
END_TEST

/* FIDs are detected _before_ the fragment is decoded */
START_TEST(rufru_id_not_a_fid)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "[%3a%3a]",
    };
    const struct rbh_id ID = {
        .data = "::",
        .size = 2,
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_ID,
        .backend = "",
        .fsname = "",
        .id = &ID,
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(uri, &URI);

    free(uri);
}
END_TEST

START_TEST(rufru_id_two_unencoded_colons_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "[::]",
    };
    const struct lu_fid fid = {0, 0, 0};
    struct rbh_id *id = rbh_id_from_lu_fid(&fid);
    const struct rbh_uri URI = {
        .type = RBH_UT_ID,
        .backend = "",
        .fsname = "",
        .id = id,
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(uri, &URI);

    free(uri);
    free(id);
}
END_TEST

START_TEST(rufru_fid_encoded_fragment)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .path = ":",
        .fragment = "[%30%78%30:0x1:%30%78%32]",
        /*            ^^^^^^^^^     ^^^^^^^^^
         *                  0x0           0x2
         */
    };
    const struct lu_fid fid = {0, 1, 2};
    struct rbh_id *id = rbh_id_from_lu_fid(&fid);
    const struct rbh_uri URI = {
        .type = RBH_UT_ID,
        .backend = "",
        .fsname = "",
        .id = id,
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(uri, &URI);

    free(uri);
    free(id);
}
END_TEST

START_TEST(rufru_username)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .userinfo = "userinfo",
        .path = ":",
    };
    struct rbh_uri_authority URI_ADDRESS = {
        .username = "userinfo",
        .password = "",
        .host = "",
        .port = 0,
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_BARE,
        .authority = &URI_ADDRESS,
        .backend = "",
        .fsname = "",
        .path = "",
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(&URI, uri);
}
END_TEST

START_TEST(rufru_username_and_password)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .userinfo = "userinfo:blob123",
        .path = ":",
    };
    struct rbh_uri_authority URI_ADDRESS = {
        .username = "userinfo",
        .password = "blob123",
        .host = "",
        .port = 0,
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_BARE,
        .authority = &URI_ADDRESS,
        .backend = "",
        .fsname = "",
        .path = "",
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(&URI, uri);
}
END_TEST

START_TEST(rufru_host)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .host = "remotehost",
        .path = ":",
    };
    struct rbh_uri_authority URI_ADDRESS = {
        .username = "",
        .password = "",
        .host = "remotehost",
        .port = 0,
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_BARE,
        .authority = &URI_ADDRESS,
        .backend = "",
        .fsname = "",
        .path = "",
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(&URI, uri);
}
END_TEST

START_TEST(rufru_host_and_port)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .host = "remotehost",
        .port = "42",
        .path = ":",
    };
    struct rbh_uri_authority URI_ADDRESS = {
        .username = "",
        .password = "",
        .host = "remotehost",
        .port = 42,
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_BARE,
        .authority = &URI_ADDRESS,
        .backend = "",
        .fsname = "",
        .path = "",
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(&URI, uri);
}
END_TEST

START_TEST(rufru_invalid_port)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .port = "invalid",
        .path = ":",
    };
    struct rbh_uri *uri;

    errno = 0;
    uri = rbh_uri_from_raw_uri(&RAW_URI);

    ck_assert_ptr_null(uri);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rufru_complete_authority)
{
    const struct rbh_raw_uri RAW_URI = {
        .scheme = RBH_SCHEME,
        .userinfo = "john:doe",
        .host = "sum",
        .port = "41",
        .path = ":",
    };
    struct rbh_uri_authority URI_ADDRESS = {
        .username = "john",
        .password = "doe",
        .host = "sum",
        .port = 41,
    };
    const struct rbh_uri URI = {
        .type = RBH_UT_BARE,
        .authority = &URI_ADDRESS,
        .backend = "",
        .fsname = "",
        .path = "",
    };
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(&RAW_URI);
    ck_assert_ptr_nonnull(uri);
    ck_assert_uri_eq(&URI, uri);
}
END_TEST

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("uri");
    tests = tcase_create("rbh_raw_uri_from_string()");
    tcase_add_test(tests, rrufs_empty);
    tcase_add_test(tests, rrufs_scheme);
    tcase_add_test(tests, rrufs_missing_scheme);
    tcase_add_test(tests, rrufs_fragment);
    tcase_add_test(tests, rrufs_empty_fragment);
    tcase_add_test(tests, rrufs_query);
    tcase_add_test(tests, rrufs_no_athority_absolute_path);
    tcase_add_test(tests, rrufs_no_athority_relative_path);
    tcase_add_test(tests, rrufs_empty_authority_empty_path);
    tcase_add_test(tests, rrufs_empty_authority_path);
    tcase_add_test(tests, rrufs_userinfo);
    tcase_add_test(tests, rrufs_host);
    tcase_add_test(tests, rrufs_port);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_percent_decode()");
    tcase_add_test(tests, rpd_every_hexa_char);
    tcase_add_test(tests, rpd_fully_encoded);
    tcase_add_test(tests, rpd_unencoded);
    tcase_add_test(tests, rpd_too_short);
    tcase_add_test(tests, rpd_not_hexa_first);
    tcase_add_test(tests, rpd_not_hexa_second);
    tcase_add_test(tests, rpd_case_insensitive);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_uri_from_raw_uri()");
    tcase_add_test(tests, rufru_wrong_scheme);
    tcase_add_test(tests, rufru_no_colon);
    tcase_add_test(tests, rufru_encoded_backend_and_fsname);
    tcase_add_test(tests, rufru_misencoded_backend);
    tcase_add_test(tests, rufru_misencoded_fsname);
    tcase_add_test(tests, rufru_no_fragment);
    tcase_add_test(tests, rufru_empty_fragment);
    tcase_add_test(tests, rufru_empty_id_fragment);
    tcase_add_test(tests, rufru_misencoded_id_fragment);
    tcase_add_test(tests, rufru_misencoded_path_fragment);
    tcase_add_test(tests, rufru_path_fragment_with_bracket);
    tcase_add_test(tests, rufru_id_fragment);
    tcase_add_test(tests, rufru_fid_fragment);
    tcase_add_test(tests, rufru_bad_fid_fragment);
    tcase_add_test(tests, rufru_fid_and_garbage_fragment);
    tcase_add_test(tests, rufru_misencoded_fid_fragment);
    tcase_add_test(tests, rufru_id_single_unencoded_colon_fragment);
    tcase_add_test(tests, rufru_id_not_a_fid);
    tcase_add_test(tests, rufru_id_two_unencoded_colons_fragment);
    tcase_add_test(tests, rufru_fid_encoded_fragment);
    tcase_add_test(tests, rufru_username);
    tcase_add_test(tests, rufru_username_and_password);
    tcase_add_test(tests, rufru_host);
    tcase_add_test(tests, rufru_host_and_port);
    tcase_add_test(tests, rufru_invalid_port);
    tcase_add_test(tests, rufru_complete_authority);

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
