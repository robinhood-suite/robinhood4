/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>
#include <stdlib.h>
#include <stdio.h>

#include <miniyaml.h>

#include "serialization.h"
#include "sink.h"

struct file_sink {
    struct sink sink;

    yaml_emitter_t emitter;
    FILE *file;
};

static int
file_sink_process(void *_sink, struct rbh_iterator *fsevents)
{
    struct file_sink *sink = _sink;

    while (true) {
        const struct rbh_fsevent *fsevent;

        fsevent = rbh_iter_next(fsevents);
        if (fsevent == NULL)
            break;

        if (!emit_fsevent(&sink->emitter, fsevent))
            return -1;
    }

    return errno == ENODATA ? 0 : -1;
}

static void
file_sink_destroy(void *_sink)
{
    struct file_sink *sink = _sink;

    yaml_emitter_delete(&sink->emitter);
    if (fclose(sink->file))
        error(EXIT_SUCCESS, errno, "sink: %s: fclose", sink->sink.name);
    free(sink);
}

static const struct sink_operations FILE_SINK_OPS = {
    .process = file_sink_process,
    .destroy = file_sink_destroy,
};

static const struct sink FILE_SINK = {
    .name = "file",
    .ops = &FILE_SINK_OPS,
};

struct sink *
sink_from_file(FILE *file)
{
    struct file_sink *sink;

    sink = malloc(sizeof(*sink));
    if (sink == NULL)
        error(EXIT_FAILURE, 0, "malloc");

    if (!yaml_emitter_initialize(&sink->emitter))
        error(EXIT_FAILURE, 0, "yaml_emitter_initialize");

    yaml_emitter_set_output_file(&sink->emitter, file);
    yaml_emitter_set_unicode(&sink->emitter, true);

    if (!yaml_emit_stream_start(&sink->emitter, YAML_UTF8_ENCODING))
        error(EXIT_FAILURE, 0, "yaml serialization error: %s",
              sink->emitter.problem);

    sink->sink = FILE_SINK;
    sink->file = file;
    return &sink->sink;
}
