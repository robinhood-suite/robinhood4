/* This file is part of RobinHood 4.
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <stdatomic.h>
#include <stdio.h>

#include "robinhood/log.h"
#include "robinhood/utils.h"

static void
print_sync_log(struct rbh_metadata *metadata, time_t current,
               FILE *log_file)
{
    uint64_t total_entry_count = metadata->sync_md.converted_entries +
                                 metadata->sync_md.skipped_entries;
    uint64_t time_spent = current - metadata->common_md.start_time;

    fprintf(log_file,
        "STATS | ======== Backend scan statistics =========\n"
        "STATS | rbh-sync is running:\n"
        "STATS |      progress: %lu entries scanned (%lu skipped)\n"
        "STATS |      current speed: %.2f entries/sec\n\n",
        total_entry_count,
        metadata->sync_md.skipped_entries,
        time_spent == 0 ? total_entry_count :
                          (double) total_entry_count  / (double) time_spent
    );
}

static void
print_gc_log(struct rbh_metadata *metadata, time_t current,
             FILE *log_file)
{
    uint64_t total_entry_count = metadata->gc_md.total_entry_count;
    uint64_t time_spent = current - metadata->common_md.start_time;

    fprintf(log_file,
        "STATS | ======== Garbage collector statistics =========\n"
        "STATS | rbh-gc is running:\n"
        "STATS |      progress: %lu entries deleted from mirror (%lu kept)\n"
        "STATS |      current speed: %.2f entries/sec\n\n",
        metadata->gc_md.deleted_entry_count,
        metadata->gc_md.total_entry_count - metadata->gc_md.deleted_entry_count,
        time_spent == 0 ? total_entry_count :
                          (double) total_entry_count  / (double) time_spent
    );
}

static void
print_fsevents_log(struct rbh_metadata *metadata, time_t current,
                   FILE *log_file)
{
    struct rbh_fsevents_metadata *fsevents_md = &metadata->fsevents_md;
    uint64_t eu_nsec =
        atomic_load(&fsevents_md->time_spent_enrich_and_update).tv_nsec;
    uint64_t eu_sec =
        atomic_load(&fsevents_md->time_spent_enrich_and_update).tv_sec;
    uint64_t rd_nsec = fsevents_md->time_spent_read_and_dedup.tv_nsec;
    uint64_t rd_sec = fsevents_md->time_spent_read_and_dedup.tv_sec;
    uint64_t time_spent = current - metadata->common_md.start_time;
    uint64_t total_rd_ns;
    uint64_t total_eu_ns;
    uint64_t avg_rd_ns;
    uint64_t avg_eu_ns;

    total_rd_ns = ((uint64_t) rd_sec * 1000000000ULL) + rd_nsec;
    total_eu_ns = ((uint64_t) eu_sec * 1000000000ULL) + eu_nsec;

    avg_rd_ns = total_rd_ns / fsevents_md->changelog_read;
    avg_eu_ns = total_eu_ns / fsevents_md->changelog_read;
    avg_eu_ns = avg_eu_ns / fsevents_md->worker_count;

    fprintf(log_file,
        "STATS | ======== Backend update statistics =========\n"
        "STATS | rbh-fsevents is running:\n"
        "STATS |      progress: %lu changelog read\n"
        "STATS |      worker: %lu\n"
        "STATS |      current speed:\n"
        "STATS |          read/dedup: %llu.%09llu changelog/sec\n"
        "STATS |          enrich/update: %llu.%09llu changelog/sec/worker\n"
        "STATS |          overall: %.2f changelog/sec\n\n",
        fsevents_md->changelog_read,
        fsevents_md->worker_count,
        avg_rd_ns / 1000000000ULL,
        avg_rd_ns % 1000000000ULL,
        avg_eu_ns / 1000000000ULL,
        avg_eu_ns % 1000000000ULL,
        time_spent == 0 ? fsevents_md->changelog_read :
                          (double) fsevents_md->changelog_read  /
                            (double) time_spent
    );
}

void
rbh_print_log(struct rbh_metadata *metadata, enum rbh_log_type command_type,
              FILE *log_file)
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

    fprintf(log_file,
        "STATS | =================== Dumping stats at %s ====================\n"
        "STATS | ======== General statistics =========\n"
        "STATS | Started command: rbh-%s\n"
        "STATS | Start time: %s (%s ago)\n",
        current_time_string,
        command,
        start_time_string,
        difftime_buffer
    );

    switch (command_type) {
    case RBH_FSEVENTS_LOG:
        print_fsevents_log(metadata, current, log_file);
        break;
    case RBH_GC_LOG:
        print_gc_log(metadata, current, log_file);
        break;
    case RBH_SYNC_LOG:
        print_sync_log(metadata, current, log_file);
        break;
    default:
        break;
    }

    metadata->last_shown_time = current;
}

void
rbh_timespec_atomic_accumulate(struct rbh_fsevents_metadata *fsevents_md,
                               struct timespec to_add) {
    struct timespec current;
    struct timespec temp;

    current = atomic_load(&fsevents_md->time_spent_enrich_and_update);

    do {
        temp = current;

        temp.tv_sec += to_add.tv_sec;
        temp.tv_nsec += to_add.tv_nsec;

        if (temp.tv_nsec >= 1000000000) {
            temp.tv_nsec -= 1000000000;
            temp.tv_sec++;
        }

        /* Atomically replace if 'temp' still matches 'current'. If it changed,
         * 'current' is updated automatically, and we loop again.
         */
    } while (!atomic_compare_exchange_weak(
        &fsevents_md->time_spent_enrich_and_update,
        &current,
        temp)
    );
}
