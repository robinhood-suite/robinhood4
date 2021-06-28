/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <error.h>
#include <stdlib.h>

#include <miniyaml.h>

#include "serialization.h"

static void __attribute__((noreturn))
parser_error(yaml_parser_t *parser)
{
    error(EXIT_FAILURE, 0, "parser error: %s", parser->problem);
    __builtin_unreachable();
}

/*----------------------------------------------------------------------------*
 |                                   upsert                                   |
 *----------------------------------------------------------------------------*/

#define UPSERT_TAG "!upsert"

static bool
emit_upsert(yaml_emitter_t *emitter __attribute__((unused)),
            const struct rbh_fsevent *upsert __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

static bool
parse_upsert(yaml_parser_t *parser __attribute__((unused)),
             struct rbh_fsevent *upsert __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

/*----------------------------------------------------------------------------*
 |                                    link                                    |
 *----------------------------------------------------------------------------*/

#define LINK_TAG "!link"

static bool
emit_link(yaml_emitter_t *emitter __attribute__((unused)),
          const struct rbh_fsevent *link __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

static bool
parse_link(yaml_parser_t *parser __attribute__((unused)),
           struct rbh_fsevent *link __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

/*----------------------------------------------------------------------------*
 |                                   unlink                                   |
 *----------------------------------------------------------------------------*/

#define UNLINK_TAG "!unlink"

static bool
emit_unlink(yaml_emitter_t *emitter __attribute__((unused)),
            const struct rbh_fsevent *unlink __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

static bool
parse_unlink(yaml_parser_t *parser __attribute__((unused)),
             struct rbh_fsevent *unlink __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

/*----------------------------------------------------------------------------*
 |                                   delete                                   |
 *----------------------------------------------------------------------------*/

#define DELETE_TAG "!delete"

static bool
emit_delete(yaml_emitter_t *emitter __attribute__((unused)),
            const struct rbh_fsevent *delete __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

static bool
parse_delete(yaml_parser_t *parser __attribute__((unused)),
             struct rbh_fsevent *delete __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

/*----------------------------------------------------------------------------*
 |                                   xattr                                    |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                             namespace                              |
     *--------------------------------------------------------------------*/

#define NS_XATTR_TAG "!ns_xattr"

static bool
parse_ns_xattr(yaml_parser_t *parser __attribute__((unused)),
               struct rbh_fsevent *ns_xattr __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

    /*--------------------------------------------------------------------*
     |                               inode                                |
     *--------------------------------------------------------------------*/

#define INODE_XATTR_TAG "!inode_xattr"

static bool
parse_inode_xattr(yaml_parser_t *parser __attribute__((unused)),
                  struct rbh_fsevent *inode_xattr __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

/*---------------------------------- xattr -----------------------------------*/

static bool
emit_xattr(yaml_emitter_t *emitter __attribute__((unused)),
           const struct rbh_fsevent *xattr __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

/*----------------------------------------------------------------------------*
 |                                  fsevent                                   |
 *----------------------------------------------------------------------------*/

bool
emit_fsevent(yaml_emitter_t *emitter, const struct rbh_fsevent *fsevent)
{
    bool success;

    if (!yaml_emit_document_start(emitter))
        return false;

    switch ((int)fsevent->type) {
    case RBH_FET_UPSERT:
        success = emit_upsert(emitter, fsevent);
        break;
    case RBH_FET_LINK:
        success = emit_link(emitter, fsevent);
        break;
    case RBH_FET_UNLINK:
        success = emit_unlink(emitter, fsevent);
        break;
    case RBH_FET_DELETE:
        success = emit_delete(emitter, fsevent);
        break;
    case RBH_FET_XATTR:
        success = emit_xattr(emitter, fsevent);
        break;
    default:
        error(EXIT_FAILURE, EINVAL, __func__);
        __builtin_unreachable();
    }

    return success && yaml_emit_document_end(emitter);
}

enum fsevent_type {
    FT_UNKNOWN,
    FT_UPSERT,
    FT_DELETE,
    FT_LINK,
    FT_UNLINK,
    FT_NS_XATTR,
    FT_INODE_XATTR,
};

static enum fsevent_type
str2fsevent_type(const char *string)
{
    /* XXX: if this ever becomes a bottleneck, you can try unrolling the calls
     *      to strcmp() with large switch statements. Right now, this feels like
     *      premature optimization (the readability is just not as good).
     */
    if (strcmp(string, UPSERT_TAG) == 0)
        return FT_UPSERT;
    if (strcmp(string, DELETE_TAG) == 0)
        return FT_DELETE;
    if (strcmp(string, LINK_TAG) == 0)
        return FT_LINK;
    if (strcmp(string, UNLINK_TAG) == 0)
        return FT_UNLINK;
    if (strcmp(string, NS_XATTR_TAG) == 0)
        return FT_NS_XATTR;
    if (strcmp(string, INODE_XATTR_TAG) == 0)
        return FT_INODE_XATTR;

    errno = EINVAL;
    return FT_UNKNOWN;
}

bool
parse_fsevent(yaml_parser_t *parser, struct rbh_fsevent *fsevent)
{
    enum fsevent_type type;
    yaml_event_t event;
    const char *tag;
    int save_errno;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    if (event.type != YAML_MAPPING_START_EVENT) {
        yaml_event_delete(&event);
        errno = EINVAL;
        return false;
    }

    tag = yaml_mapping_tag(&event);
    if (tag == NULL) {
        yaml_event_delete(&event);
        errno = EINVAL;
        return false;
    }

    type = str2fsevent_type(tag);
    save_errno = errno;
    yaml_event_delete(&event);

    switch (type) {
    case FT_UNKNOWN:
        errno = save_errno;
        return false;
    case FT_UPSERT:
        fsevent->type = RBH_FET_UPSERT;
        return parse_upsert(parser, fsevent);
    case FT_DELETE:
        fsevent->type = RBH_FET_DELETE;
        return parse_delete(parser, fsevent);
    case FT_LINK:
        fsevent->type = RBH_FET_LINK;
        return parse_link(parser, fsevent);
    case FT_UNLINK:
        fsevent->type = RBH_FET_UNLINK;
        return parse_unlink(parser, fsevent);
    case FT_NS_XATTR:
        fsevent->type = RBH_FET_XATTR;
        return parse_ns_xattr(parser, fsevent);
    case FT_INODE_XATTR:
        fsevent->type = RBH_FET_XATTR;
        return parse_inode_xattr(parser, fsevent);
    default:
        assert(false);
        __builtin_unreachable();
    }
}
