/**
 * @file        wsm_env.h
 * @brief       Environment variable parsing helpers for wlframe.
 * @details     This file provides small helpers for reading boolean and
 *              enumerated switch values from environment variables.
 * @author      YaoBing Xiao
 * @date        2026-07-21
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-07-21, initial version\n
 */

#ifndef UTIL_WSM_ENV_H
#define UTIL_WSM_ENV_H

#include <stdbool.h>
#include <unistd.h>

/**
 * @brief Parses a boolean from an environment variable.
 *
 * @param option Name of the environment variable.
 * @return true when the variable is set to "1", false when unset, set to "0",
 *         or set to an unknown value.
 */
bool env_parse_bool(const char *option);

/**
 * @brief Picks a switch choice from an environment variable.
 *
 * @param option Name of the environment variable.
 * @param switches NULL-terminated array of accepted string values.
 * @return Index of the matched switch, or 0 when unset or unknown.
 */
size_t env_parse_switch(const char *option, const char **switches);

#endif // UTIL_WSM_ENV_H
