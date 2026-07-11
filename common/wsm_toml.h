#ifndef WSM_TOML_H
#define WSM_TOML_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <wayland-util.h>

enum wsm_toml_type {
	WSM_TOML_STRING,
	WSM_TOML_INTEGER,
	WSM_TOML_DOUBLE,
	WSM_TOML_BOOLEAN,
};

struct wsm_toml_entry {
	struct wl_list link;
	char *key;
	enum wsm_toml_type type;
	union {
		char *string;
		int64_t integer;
		double decimal;
		bool boolean;
	} value;
};

struct wsm_toml {
	struct wl_list entries; // wsm_toml_entry::link
};

struct wsm_toml *wsm_toml_create(void);
struct wsm_toml *wsm_toml_load(
	const char *path, char *error, size_t error_size);
void wsm_toml_destroy(struct wsm_toml *toml);

bool wsm_toml_has(const struct wsm_toml *toml, const char *key);
const struct wsm_toml_entry *wsm_toml_get_entry(
	const struct wsm_toml *toml, const char *key);
bool wsm_toml_get_string(
	const struct wsm_toml *toml, const char *key, char **value);
bool wsm_toml_get_int(
	const struct wsm_toml *toml, const char *key, int64_t *value);
bool wsm_toml_get_double(
	const struct wsm_toml *toml, const char *key, double *value);
bool wsm_toml_get_bool(
	const struct wsm_toml *toml, const char *key, bool *value);

bool wsm_toml_set_string(
	struct wsm_toml *toml, const char *key, const char *value);
bool wsm_toml_set_int(struct wsm_toml *toml, const char *key, int64_t value);
bool wsm_toml_set_double(struct wsm_toml *toml, const char *key, double value);
bool wsm_toml_set_bool(struct wsm_toml *toml, const char *key, bool value);
bool wsm_toml_remove(struct wsm_toml *toml, const char *key);

/**
 * Atomically replace path with the serialized document.
 */
bool wsm_toml_save(const struct wsm_toml *toml, const char *path);

#endif
