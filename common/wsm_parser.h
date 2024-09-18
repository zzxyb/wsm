#ifndef WSM_PARSER_H
#define WSM_PARSER_H

#include <stdbool.h>
#include <limits.h>

#include <wayland-util.h>

struct wsm_config_entry {
	struct wl_list link;
	char *key;
	char *value;
};

struct wsm_config_section {
	struct wl_list entry_list;
	struct wl_list link;
	char *name;
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
