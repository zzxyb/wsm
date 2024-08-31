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

#ifndef WSM_PARSER_H
#define WSM_PARSER_H

#include <stdbool.h>
#include <limits.h>

#include <wayland-util.h>

struct wsm_config_entry {
	char *key;
	char *value;
	struct wl_list link;
};

struct wsm_config_section {
	char *name;
	struct wl_list entry_list;
	struct wl_list link;
};

enum wsm_option_type {
	WSM_OPTION_INTEGER,
	WSM_OPTION_UNSIGNED_INTEGER,
	WSM_OPTION_STRING,
	WSM_OPTION_BOOLEAN
};

struct wsm_option {
	enum wsm_option_type type;
	const char *name;
	char short_name;
	void *data;
};

struct wsm_config {
	struct wl_list section_list;
	char path[PATH_MAX];
};

int parse_options(const struct wsm_option *options, int count, int *argc, char *argv[]);
struct wsm_config_section *weston_config_get_section(struct wsm_config *config, const char *section,
													 const char *key, const char *value);
int wsm_config_section_get_int(struct wsm_config_section *section, const char *key,
							   int32_t *value, int32_t default_value);
int wsm_config_section_get_uint(struct wsm_config_section *section, const char *key,
								uint32_t *value, uint32_t default_value);
int wsm_config_section_get_double(struct wsm_config_section *section, const char *key,
								  double *value, double default_value);
int wsm_config_section_get_string(struct wsm_config_section *section, const char *key, char **value,
								  const char *default_value);
int wsm_config_section_get_bool(struct wsm_config_section *section, const char *key,
								bool *value, bool default_value);

#endif
