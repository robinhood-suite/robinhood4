/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <uuid/uuid.h>
#include <sys/utsname.h>

#include "robinhood/uri.h"
#include "robinhood/utils.h"

/**
 * When executing an external processes, two I/O channels are open on its
 * stdout / stderr streams.  Everytime a line is read from these channels
 * we call a user-provided function back.
 */
struct io_chan_arg {
    struct exec_ctx *exec_ctx;
    parse_cb_t cb;
    void *udata;
    int ident;
};

/**
 * GMainLoop exposes a refcount but it is not related to running and stopping
 * the loop. Because we can have several users of the loop (child process
 * termination watcher, stdout watcher, stderr watcher), we need to wait for
 * all of them to complete before calling g_main_loop_quit(). Use custom
 * reference counting for this purpose.
 */
struct exec_ctx {
    GMainLoop *loop;  /* GMainLoop for the current context */
    int ref;          /* Pending operations in the loop */
    int rc;           /* Subprocess termination code (as an errno) */
};

/**
 * Increment reference count (in term of pending operations on the loop)
 */
static inline void
ctx_incref(struct exec_ctx *ctx)
{
    assert(ctx->ref >= 0);
    ctx->ref++;
}

/**
 * Decrement reference count (in term of pending operations on the loop).
 * Quit the loop but without freeing it if count reaches zero.
 * Allocated will be released by the caller.
 */
static inline void
ctx_decref(struct exec_ctx *ctx)
{
    assert(ctx->ref > 0);
    if (--ctx->ref == 0)
        g_main_loop_quit(ctx->loop);
}

#ifndef g_spawn_check_wait_status
#define g_spawn_check_wait_status g_spawn_check_exit_status
#endif

/**
 * External process termination handler.
 */
static void
watch_child_cb(GPid pid, gint status, gpointer data)
{
    struct exec_ctx *ctx = data;
    const char *error_string;

    if (status != 0) {
        GError *error = NULL;

        g_spawn_check_wait_status(status, &error);
        error_string = error->message;
        ctx->rc = error->code;

        fprintf(stderr, "External command failed: %s", error_string);
    }

    g_spawn_close_pid(pid);
    ctx_decref(ctx);
}

/**
 * IO channel watcher.
 * Read one line from the current channel and forward it to the user function.
 *
 * Return true as long as the channel has to stay registered, false otherwise.
 */
static gboolean
readline_cb(GIOChannel *channel, GIOCondition cond, gpointer ud)
{
    struct io_chan_arg  *args  = ud;
    GError *error = NULL;
    GIOStatus res;
    gchar *line;
    gsize size;

    /* The channel is closed, no more data to read */
    if (cond == G_IO_HUP) {
        g_io_channel_unref(channel);
        ctx_decref(args->exec_ctx);
        return false;
    }

    res = g_io_channel_read_line(channel, &line, &size, NULL, &error);
    switch (res) {
    case G_IO_STATUS_EOF:
        g_io_channel_shutdown(channel, true, &error);
        g_error_free(error);
        g_io_channel_unref(channel);
        ctx_decref(args->exec_ctx);

        return false;
    case G_IO_STATUS_ERROR:
        if (error) {
            fprintf(stderr, "Cannot read from child: %s", error->message);
            g_error_free(error);
        } else {
            fprintf(stderr, "Cannot read from child, but no error was given");
        }

        return false;
    case G_IO_STATUS_NORMAL:
        args->cb(args->udata, line, size, args->ident);
        g_free(line);
    case G_IO_STATUS_AGAIN:
        return true;
    }

     __builtin_unreachable();
    return true;
}

/**
 * Execute synchronously an external command, read its output and invoke
 * a user-provided filter function on every line of it.
 */
int
command_call(const char *cmd_line, parse_cb_t cb_func, void *cb_arg)
{
    GSpawnFlags flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;
    struct exec_ctx ctx = { 0 };
    GError *err_desc = NULL;
    GIOChannel *out_chan;
    GIOChannel *err_chan;
    gchar **av = NULL;
    int p_stdout;
    int p_stderr;
    bool success;
    int rc = 0;
    GPid pid;
    gint ac;

    success = g_shell_parse_argv(cmd_line, &ac, &av, &err_desc);
    if (!success) {
        fprintf(stderr, "Cannot parse '%s' : '%s'",
                cmd_line, err_desc->message);
        goto out_err_free;
    }

    ctx.loop = g_main_loop_new(NULL, false);
    ctx.ref = 0;
    ctx.rc = 0;

    success = g_spawn_async_with_pipes(NULL,   /* Working dir */
                                       av,     /* Parameters */
                                       NULL,   /* Environment */
                                       flags,  /* Execution directives */
                                       NULL,   /* Child setup function */
                                       NULL,   /* Child setup arg */
                                       &pid,   /* Child PID */
                                       NULL,      /* STDIN (unused) */
                                       cb_func ? &p_stdout : NULL, /* STDOUT */
                                       cb_func ? &p_stderr : NULL, /* STDERR */
                                       &err_desc);
    if (!success) {
        fprintf(stderr, "Failed to execute '%s' : '%s'",
                cmd_line, err_desc->message);
        goto out_free;
    }

    ctx_incref(&ctx);
    g_child_watch_add(pid, watch_child_cb, &ctx);

    if (cb_func != NULL) {
        struct io_chan_arg  out_args = {
            .ident    = STDOUT_FILENO,
            .cb       = cb_func,
            .udata    = cb_arg,
            .exec_ctx = &ctx
        };
        struct io_chan_arg  err_args = {
            .ident    = STDERR_FILENO,
            .cb       = cb_func,
            .udata    = cb_arg,
            .exec_ctx = &ctx
        };

        out_chan = g_io_channel_unix_new(p_stdout);
        err_chan = g_io_channel_unix_new(p_stderr);

        /* instruct the refcount system to close the channels when unused */
        g_io_channel_set_close_on_unref(out_chan, true);
        g_io_channel_set_close_on_unref(err_chan, true);

        /* update refcount for the two watchers */
        ctx_incref(&ctx);
        ctx_incref(&ctx);

        g_io_add_watch(out_chan, G_IO_IN | G_IO_HUP, readline_cb, &out_args);
        g_io_add_watch(err_chan, G_IO_IN | G_IO_HUP, readline_cb, &err_args);
    }

    g_main_loop_run(ctx.loop);

out_free:
    g_main_loop_unref(ctx.loop);
    g_strfreev(av);

out_err_free:
    if (err_desc)
        g_error_free(err_desc);

    return rc ? rc : ctx.rc;
}
