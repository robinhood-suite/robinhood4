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

#endif
