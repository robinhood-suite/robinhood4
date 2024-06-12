/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
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
 * Create and initialize the config.
 *
 * Two `parse` calls are done to skip the initial YAML_STREAM_START_EVENT and
 * YAML_DOCUMENT_START_EVENT.
 *
 * @param config_file   the path to the configuration file to use
 *
 * @return              0 on success, non-zero otherwise
 */
int
rbh_config_open(const char *config_file);

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
 * @param key           the key to search, can be of the form `a/b` to search
 *                      a subkey
 * @param value         the value corresponding to the key, its type is set to
 *                      -1 if the key was not found
 *
 * @return              a non-zero error code if scalar parsing of the key
 *                      failed or an unknown event was found in the
 *                      configuration, and errno is set
 *                      0 otherwise
 */
int
rbh_config_find(const char *key, struct rbh_value *value);

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

#endif
