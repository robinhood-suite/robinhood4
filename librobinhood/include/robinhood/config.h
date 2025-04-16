/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_CONFIG_H
#define ROBINHOOD_CONFIG_H

#include <yaml.h>

#include "robinhood/value.h"

struct rbh_config;

/**
 * Here are defined common configuration keys for use across multiple tools and
 * backends.
 */
#define XATTR_EXPIRES_KEY "RBH_RETENTION_XATTR"

enum key_parse_result {
    KPR_FOUND,
    KPR_NOT_FOUND,
    KPR_ERROR,
};

/**
 * Create and initialize the config.
 *
 * Two `parse` calls are done to skip the initial YAML_STREAM_START_EVENT and
 * YAML_DOCUMENT_START_EVENT.
 *
 * @param config_file   the path to the configuration file to use
 *
 * @return              0 on success, -1 on error and set errno accordingly
 */
int
rbh_config_open(const char *config_file);

/**
 * Try to open the configuration from CLI args. If no config arg is given, try
 * to find it from the environment variable RBH_CONFIG_PATH. Finally, try to
 * open the default configuration from "/etc/robinhood4.d/default.yaml"
 *
 * @return       0 on success, -1 on error and set errno accordingly
 */
int
rbh_config_from_args(int argc, char **argv);

/**
 * Return the value of RBH_CONFIG_PATH
 */
const char *
rbh_config_env_name(void);

/**
 * Free the config.
 */
void
rbh_config_free();

/**
 * Reset the config.
 *
 * Re-initialize the parser to the start of the file and skip the first two
 * events.
 *
 * @return              0 if the reset succeeded, non-zero code otherwise
 */
int
rbh_config_reset();

/**
 * Search a key in the configuration file and return the value associated to
 * that key.
 *
 * Must be called after a call to `rbh_config_open` or `rbh_config_reset`.
 *
 * This call can search keys in submaps or sequences, by calling it with a key
 * `a/b/c`, where `a` is a map containing `b` which is another
 * map containing the key `c`.
 *
 * @param key            the key to search, can be of the form `a/b` to search
 *                       a subkey
 * @param value          the value corresponding to the key
 * @param expected_type  the type expected to be found in the configuration
 *                       file. If the key isn't found in the configuration file,
 *                       and the expected type is not a string, will not fetch
 *                       the value of the environement variable
 *
 * @return               KPR_FOUND if the key was found,
 *                       KPR_NOT_FOUND if the key wasn't found,
 *                       KPR_ERROR if there was an error while parsing the
 *                       configuration file, and errno is set
 */
enum key_parse_result
rbh_config_find(const char *key, struct rbh_value *value,
                enum rbh_value_type expected_type);

/**
 * Return the local config.
 *
 * @return              the local config
 */
struct rbh_config *
get_rbh_config();

/**
 * Load a given config as the local one.
 *
 * @param config        the config to load
 */
void
load_rbh_config(struct rbh_config *config);

/**
 * Get a string from the configuration file.
 *
 * @param key             the key to search
 * @param default_string  the string to default to if the key is not found in
 *                        the configuration file
 *
 * @return              a string associated to the given key if present in the
 *                      configuration file, the given default if not, or NULL if
 *                      an error occured
 */
const char *
rbh_config_get_string(const char *key, const char *default_string);

/**
 * Get the plugin that is extended by a given backend.
 *
 * @param backend   the backend from which to output the extended plugin
 *
 * @return          the extended plugin, or the backend itself if it corresponds
 *                  to an actual plugin
 */
const char *
rbh_config_get_extended_plugin(const char *backend);

#endif
