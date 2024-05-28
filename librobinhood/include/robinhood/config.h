/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ROBINHOOD_CONFIG_H
#define ROBINHOOD_CONFIG_H

#include <stdio.h>
#include <miniyaml.h>

struct rbh_config {
    FILE *file;
    yaml_parser_t parser;
};

/**
 * Create and initialize a rbh_config structure.
 *
 * Two `parse` calls are done to skip the initial YAML_STREAM_START_EVENT and
 * YAML_DOCUMENT_START_EVENT.
 *
 * This function will error out the program if it fails at any point.
 *
 * @param config_file   the path to the configuration file to use
 *
 * @return              a pointer to a struct rbh_config properly initialized
 */
struct rbh_config *
rbh_config_initialize(const char *config_file);

/**
 * Reset a rbh_config structure.
 *
 * Re-initialize the parser to the start of the file and skip the first two
 * events.
 *
 * This function will error out the program if it fails at any point.
 *
 * @param config        the rbh_config to reset
 */
void
rbh_config_reset(struct rbh_config *config);

/**
 * Search a key in the configuration file and return the event associated to the
 * value of that key.
 *
 * Should be called after a call to `rbh_config_initialize` or
 * `rbh_config_reset`.
 *
 * This call can search keys in submaps or sequences, by calling it with a key
 * `a/b/c`, where `a` is a map/sequence containing `b` which is another
 * map/sequence containing the key `c`.
 *
 * This function will error out the program if it fails parsing events.
 *
 * @param config        the configuration in which to search the key
 * @param key           the key to search, can be of the form `a/b` to search
 *                      a subkey
 * @param event         the event corresponding to the key, NULL if the key was
 *                      not found
 *
 * @return              a non-zero error code if scalar parsing of the key
 *                      failed or an unknown event was found in the
 *                      configuration, and errno is set
 *                      0 otherwise
 */
int
rbh_config_search(struct rbh_config *config, const char *key,
                  yaml_event_t *event);

#endif
