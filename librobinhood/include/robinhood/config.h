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

#endif
