/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_CONFIG_H
#define ROBINHOOD_CONFIG_H

/**
 * Create and initialize the config.
 *
 * Two `parse` calls are done to skip the initial YAML_STREAM_START_EVENT and
 * YAML_DOCUMENT_START_EVENT.
 *
 * @return              0 on success, non-zero code otherwise
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
 * @return              0 if the reset suceeded, non-zero code otherwise
 */
int
rbh_config_reset();

/**
 * Search a key in the configuration file and return the event associated to the
 * value of that key.
 *
 * Must be called after a call to `rbh_config_open` or `rbh_config_reset`.
 *
 * This call can search keys in submaps or sequences, by calling it with a key
 * `a/b/c`, where `a` is a map containing `b` which is another
 * map containing the key `c`.
 *
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
rbh_config_find(const char *key, yaml_event_t **event);

#endif
