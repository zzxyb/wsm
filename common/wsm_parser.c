/*
MIT License

Copyright (c) 2024 YaoBing Xiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "wsm_parser.h"
#include "wsm_common.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static bool handle_option(const struct wsm_option *option, char *value) {
    char* p;

    switch (option->type) {
    case WSM_OPTION_INTEGER:
        if (!safe_strtoint(value, option->data))
            return false;
        return true;
    case WSM_OPTION_UNSIGNED_INTEGER:
        errno = 0;
        * (uint32_t *) option->data = strtoul(value, &p, 10);
        if (errno != 0 || p == value || *p != '\0')
            return false;
        return true;
    case WSM_OPTION_STRING:
        * (char **) option->data = strdup(value);
        return true;
    default:
        assert(0);
        return false;
    }
}

static bool long_option(const struct wsm_option *options, int count, char *arg) {
    int k, len;

    for (k = 0; k < count; k++) {
        if (!options[k].name)
            continue;

        len = strlen(options[k].name);
        if (strncmp(options[k].name, arg + 2, len) != 0)
            continue;

        if (options[k].type == WSM_OPTION_BOOLEAN) {
            if (!arg[len + 2]) {
                * (bool *) options[k].data = true;

                return true;
            }
        } else if (arg[len+2] == '=') {
            return handle_option(options + k, arg + len + 3);
        }
    }

    return false;
}

static bool long_option_with_arg(const struct wsm_option *options, int count, char *arg,
                                 char *param) {
    int k, len;

    for (k = 0; k < count; k++) {
        if (!options[k].name)
            continue;

        len = strlen(options[k].name);
        if (strncmp(options[k].name, arg + 2, len) != 0)
            continue;

        /* Since long_option() should handle all booleans, we should
         * never reach this
         */
        assert(options[k].type != WSM_OPTION_BOOLEAN);

        return handle_option(options + k, param);
    }

    return false;
}

static bool short_option(const struct wsm_option *options, int count, char *arg) {
    int k;

    if (!arg[1])
        return false;

    for (k = 0; k < count; k++) {
        if (options[k].short_name != arg[1])
            continue;

        if (options[k].type == WSM_OPTION_BOOLEAN) {
            if (!arg[2]) {
                * (bool *) options[k].data = true;

                return true;
            }
        } else if (arg[2]) {
            return handle_option(options + k, arg + 2);
        } else {
            return false;
        }
    }

    return false;
}

static bool short_option_with_arg(const struct wsm_option *options, int count, char *arg, char *param) {
    int k;

    if (!arg[1])
        return false;

    for (k = 0; k < count; k++) {
        if (options[k].short_name != arg[1])
            continue;

        if (options[k].type == WSM_OPTION_BOOLEAN)
            continue;

        return handle_option(options + k, param);
    }

    return false;
}

static struct wsm_config_entry *config_section_get_entry(struct wsm_config_section *section,
                                                         const char *key) {
    struct wsm_config_entry *e;

    if (section == NULL)
        return NULL;
    wl_list_for_each(e, &section->entry_list, link)
        if (strcmp(e->key, key) == 0)
        return e;

    return NULL;
}

int parse_options(const struct wsm_option *options, int count, int *argc, char *argv[]) {
    int i, j;

    for (i = 1, j = 1; i < *argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == '-') {
                /* Long option, e.g. --foo or --foo=bar */
                if (long_option(options, count, argv[i]))
                    continue;

                /* ...also handle --foo bar */
                if (i + 1 < *argc &&
                    long_option_with_arg(options, count,
                                         argv[i], argv[i+1])) {
                    i++;
                    continue;
                }
            } else {
                /* Short option, e.g -f or -f42 */
                if (short_option(options, count, argv[i]))
                    continue;

                /* ...also handle -f 42 */
                if (i+1 < *argc &&
                    short_option_with_arg(options, count, argv[i], argv[i+1])) {
                    i++;
                    continue;
                }
            }
        }
        argv[j++] = argv[i];
    }
    argv[j] = NULL;
    *argc = j;

    return j;
}

int wsm_config_section_get_int(struct wsm_config_section *section, const char *key, int32_t *value, int32_t default_value) {
    struct wsm_config_entry *entry;

    entry = config_section_get_entry(section, key);
    if (entry == NULL) {
        *value = default_value;
        errno = ENOENT;
        return -1;
    }

    if (!safe_strtoint(entry->value, value)) {
        *value = default_value;
        return -1;
    }

    return 0;
}

int wsm_config_section_get_uint(struct wsm_config_section *section, const char *key, uint32_t *value, uint32_t default_value) {
    long int ret;
    struct wsm_config_entry *entry;
    char *end;

    entry = config_section_get_entry(section, key);
    if (entry == NULL) {
        *value = default_value;
        errno = ENOENT;
        return -1;
    }

    errno = 0;
    ret = strtol(entry->value, &end, 0);
    if (errno != 0 || end == entry->value || *end != '\0') {
        *value = default_value;
        errno = EINVAL;
        return -1;
    }

    /* check range */
    if (ret < 0 || ret > INT_MAX) {
        *value = default_value;
        errno = ERANGE;
        return -1;
    }

    *value = ret;

    return 0;
}

int wsm_config_section_get_double( struct wsm_config_section *section, const char *key, double *value, double default_value) {
    struct wsm_config_entry *entry;
    char *end;

    entry = config_section_get_entry(section, key);
    if (entry == NULL) {
        *value = default_value;
        errno = ENOENT;
        return -1;
    }

    *value = strtod(entry->value, &end);
    if (*end != '\0') {
        *value = default_value;
        errno = EINVAL;
        return -1;
    }

    return 0;
}

int wsm_config_section_get_string(struct wsm_config_section *section, const char *key, char **value, const char *default_value) {
    struct wsm_config_entry *entry;

    entry = config_section_get_entry(section, key);
    if (entry == NULL) {
        if (default_value)
            *value = strdup(default_value);
        else
            *value = NULL;
        errno = ENOENT;
        return -1;
    }

    *value = strdup(entry->value);

    return 0;
}

int wsm_config_section_get_bool(struct wsm_config_section *section, const char *key, bool *value, bool default_value) {
    struct wsm_config_entry *entry;

    entry = config_section_get_entry(section, key);
    if (entry == NULL) {
        *value = default_value;
        errno = ENOENT;
        return -1;
    }

    if (strcmp(entry->value, "false") == 0)
        *value = false;
    else if (strcmp(entry->value, "true") == 0)
        *value = true;
    else {
        *value = default_value;
        errno = EINVAL;
        return -1;
    }

    return 0;
}

struct wsm_config_section *weston_config_get_section(struct wsm_config *config, const char *section, const char *key, const char *value) {
    struct wsm_config_section *s;
    struct wsm_config_entry *e;

    if (config == NULL)
        return NULL;
    wl_list_for_each(s, &config->section_list, link) {
        if (strcmp(s->name, section) != 0)
            continue;
        if (key == NULL)
            return s;
        e = config_section_get_entry(s, key);
        if (e && strcmp(e->value, value) == 0)
            return s;
    }

    return NULL;
}
