/* This file is part of RobinHood 4.
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <stdio.h>

#include "robinhood/log.h"
#include "robinhood/utils.h"

static void
print_sync_log(struct rbh_metadata *metadata, time_t current)
{
    uint64_t total_entry_count = metadata->sync_md.converted_entries +
                                 metadata->sync_md.skipped_entries;
    uint64_t time_spent = current - metadata->common_md.start_time;

    printf(
        "STATS |      progress: %lu entries scanned (%lu skipped)\n"
        "STATS |      current speed: %.2f entries/sec\n\n",
        total_entry_count,
        metadata->sync_md.skipped_entries,
        time_spent == 0 ? total_entry_count :
                          (double) total_entry_count  / (double) time_spent
    );
}

void
rbh_print_log(struct rbh_metadata *metadata, enum rbh_log_type command_type)
{
    const char *command = rbh_log_type2str(command_type);
    char current_time_string[128];
    char start_time_string[128];
    time_t current = time(NULL);
    char difftime_buffer[32];
    struct tm *current_tm;
    struct tm *start_tm;

    current_tm = localtime(&current);
    if (!strftime(current_time_string, sizeof(current_time_string),
                  "%d/%m/%Y %H:%M:%S", current_tm)) {
        fprintf(stderr,
                "Cannot print log for current command, converion of current timestamp failed");
        return;
    }

    start_tm = localtime(&metadata->common_md.start_time);
    if (!strftime(start_time_string, sizeof(start_time_string),
                  "%d/%m/%Y %H:%M:%S", start_tm)) {
        fprintf(stderr,
                "Cannot print log for current command, converion of start timestamp failed");
        return;
    }

    difftime_printer(difftime_buffer, sizeof(difftime_buffer),
                     current - metadata->common_md.start_time);

    printf(
        "STATS | =================== Dumping stats at %s ====================\n"
        "STATS | ======== General statistics =========\n"
        "STATS | Started command: rbh-%s\n"
        "STATS | Start time: %s (%s ago)\n"
        "STATS | ======== FS scan statistics =========\n"
        "STATS | rbh-%s is running:\n",
        current_time_string,
        command,
        start_time_string,
        difftime_buffer,
        command
    );

    switch (command_type) {
    case RBH_SYNC_LOG:
        print_sync_log(metadata, current);
        break;
    default:
        break;
    }

    metadata->last_shown_time = current;
}
