#include "wsm_brightness.h"

#include "wsm_common.h"
#include "wsm_log.h"
#include "wsm_output.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <wlr/types/wlr_output.h>

struct brightness_state_entry {
	char *key;
	long value;
	struct brightness_state_entry *next;
};

static char *brightness_state_dir(void) {
	const char *xdg_state_home = getenv("XDG_STATE_HOME");
	if (xdg_state_home && xdg_state_home[0] == '/') {
		char *dir = NULL;
		str_printf(&dir, "%s/wsm", xdg_state_home);
		return dir;
	}

	const char *home = getenv("HOME");
	if (!home || home[0] != '/') {
		return NULL;
	}
	char *dir = NULL;
	str_printf(&dir, "%s/.local/state/wsm", home);
	return dir;
}

static char *brightness_state_path(void) {
	char *dir = brightness_state_dir();
	if (!dir) {
		return NULL;
	}
	char *path = NULL;
	str_printf(&path, "%s/brightness.toml", dir);
	free(dir);
	return path;
}

static bool mkdir_parents(const char *path) {
	char *copy = strdup(path);
	if (!copy) {
		return false;
	}

	for (char *p = copy + 1; *p; ++p) {
		if (*p != '/') {
			continue;
		}
		*p = '\0';
		if (mkdir(copy, 0700) < 0 && errno != EEXIST) {
			free(copy);
			return false;
		}
		*p = '/';
	}
	bool ok = mkdir(copy, 0700) == 0 || errno == EEXIST;
	free(copy);
	return ok;
}

static char *brightness_state_key(struct wsm_output *output) {
	struct wlr_output *wlr_output = output ? output->wlr_output : NULL;
	if (!wlr_output) {
		return NULL;
	}

	const char *make = wlr_output->make ? wlr_output->make : "";
	const char *model = wlr_output->model ? wlr_output->model : "";
	const char *serial = wlr_output->serial ? wlr_output->serial : "";
	const char *name = wlr_output->name ? wlr_output->name : "";
	char *key = NULL;
	if (serial[0]) {
		str_printf(&key, "%s|%s|%s", make, model, serial);
	} else {
		str_printf(&key, "%s|%s|%s", make, model, name);
	}
	return key;
}

static void brightness_state_free(struct brightness_state_entry *entries) {
	while (entries) {
		struct brightness_state_entry *next = entries->next;
		free(entries->key);
		free(entries);
		entries = next;
	}
}

static char *parse_toml_string(const char **cursor) {
	const char *p = *cursor;
	if (*p++ != '"') {
		return NULL;
	}

	char *value = calloc(strlen(p) + 1, 1);
	if (!value) {
		return NULL;
	}
	char *out = value;
	while (*p && *p != '"') {
		if (*p == '\\') {
			++p;
			switch (*p) {
			case 'n': *out++ = '\n'; break;
			case 'r': *out++ = '\r'; break;
			case 't': *out++ = '\t'; break;
			case 'b': *out++ = '\b'; break;
			case 'f': *out++ = '\f'; break;
			case '"': *out++ = '"'; break;
			case '\\': *out++ = '\\'; break;
			default:
				free(value);
				return NULL;
			}
			if (*p) {
				++p;
			}
		} else {
			*out++ = *p++;
		}
	}
	if (*p != '"') {
		free(value);
		return NULL;
	}
	*cursor = p + 1;
	return value;
}

static struct brightness_state_entry *brightness_state_load(void) {
	char *path = brightness_state_path();
	if (!path) {
		return NULL;
	}
	FILE *file = fopen(path, "r");
	free(path);
	if (!file) {
		return NULL;
	}

	struct brightness_state_entry *entries = NULL;
	char *line = NULL;
	size_t capacity = 0;
	bool in_outputs = false;
	while (getline(&line, &capacity, file) >= 0) {
		const char *p = line;
		while (isspace((unsigned char)*p)) {
			++p;
		}
		if (*p == '[') {
			in_outputs = strncmp(p, "[outputs]", 9) == 0;
			continue;
		}
		if (!in_outputs || *p != '"') {
			continue;
		}

		char *key = parse_toml_string(&p);
		if (!key) {
			continue;
		}
		while (isspace((unsigned char)*p)) {
			++p;
		}
		if (*p++ != '=') {
			free(key);
			continue;
		}
		while (isspace((unsigned char)*p)) {
			++p;
		}
		errno = 0;
		char *end = NULL;
		long value = strtol(p, &end, 10);
		if (errno || end == p) {
			free(key);
			continue;
		}

		struct brightness_state_entry *entry = calloc(1, sizeof(*entry));
		if (!entry) {
			free(key);
			continue;
		}
		entry->key = key;
		entry->value = value;
		entry->next = entries;
		entries = entry;
	}
	free(line);
	fclose(file);
	return entries;
}

static void write_toml_string(FILE *file, const char *value) {
	fputc('"', file);
	for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
		switch (*p) {
		case '\\': fputs("\\\\", file); break;
		case '"': fputs("\\\"", file); break;
		case '\n': fputs("\\n", file); break;
		case '\r': fputs("\\r", file); break;
		case '\t': fputs("\\t", file); break;
		case '\b': fputs("\\b", file); break;
		case '\f': fputs("\\f", file); break;
		default:
			fputc(*p < 0x20 ? '?' : *p, file);
			break;
		}
	}
	fputc('"', file);
}

static bool brightness_state_write(struct brightness_state_entry *entries) {
	char *dir = brightness_state_dir();
	char *path = brightness_state_path();
	if (!dir || !path || !mkdir_parents(dir)) {
		free(dir);
		free(path);
		return false;
	}
	free(dir);

	char *temporary = NULL;
	str_printf(&temporary, "%s.tmp.XXXXXX", path);
	if (!temporary) {
		free(path);
		return false;
	}
	int fd = mkstemp(temporary);
	if (fd < 0) {
		free(temporary);
		free(path);
		return false;
	}
	fchmod(fd, 0600);
	FILE *file = fdopen(fd, "w");
	if (!file) {
		close(fd);
		unlink(temporary);
		free(temporary);
		free(path);
		return false;
	}

	fputs("# Managed by wsm.\n[outputs]\n", file);
	for (struct brightness_state_entry *entry = entries;
			entry; entry = entry->next) {
		write_toml_string(file, entry->key);
		fprintf(file, " = %ld\n", entry->value);
	}
	bool ok = fflush(file) == 0;
	if (ok) {
		ok = fsync(fd) == 0;
	}
	if (fclose(file) != 0) {
		ok = false;
	}
	if (ok) {
		ok = rename(temporary, path) == 0;
	}
	if (!ok) {
		unlink(temporary);
	}
	free(temporary);
	free(path);
	return ok;
}

static void brightness_state_save(struct wsm_brightness *brightness) {
	if (!brightness->state_key) {
		return;
	}
	struct brightness_state_entry *entries = brightness_state_load();
	struct brightness_state_entry *entry = entries;
	while (entry && strcmp(entry->key, brightness->state_key) != 0) {
		entry = entry->next;
	}
	if (!entry) {
		entry = calloc(1, sizeof(*entry));
		if (!entry) {
			brightness_state_free(entries);
			return;
		}
		entry->key = strdup(brightness->state_key);
		if (!entry->key) {
			free(entry);
			brightness_state_free(entries);
			return;
		}
		entry->next = entries;
		entries = entry;
	}
	entry->value = brightness->brightness;
	if (!brightness_state_write(entries)) {
		wsm_log(WSM_ERROR, "Could not persist brightness state");
	}
	brightness_state_free(entries);
}

void wsm_brightness_init(struct wsm_brightness *brightness,
		const struct wsm_brightness_impl *impl, struct wsm_output *output,
		enum wsm_brightness_method method) {
	assert(brightness);
	assert(impl);

	brightness->impl = impl;
	brightness->output = output;
	brightness->brightness = -1;
	brightness->min_brightness = 0;
	brightness->max_brightness = -1;
	brightness->method = method;
	brightness->state_key = brightness_state_key(output);
}

void wsm_brightness_destroy(struct wsm_brightness *brightness) {
	if (!brightness) {
		return;
	}
	assert(brightness->impl && brightness->impl->destroy);
	free(brightness->state_key);
	brightness->state_key = NULL;
	brightness->impl->destroy(brightness);
}

bool wsm_brightness_set(struct wsm_brightness *brightness, long value) {
	if (!brightness || !brightness->impl->set_brightness) {
		return false;
	}
	if (value < brightness->min_brightness || value > brightness->max_brightness) {
		return false;
	}
	if (!brightness->impl->set_brightness(brightness, value)) {
		return false;
	}
	brightness->brightness = value;
	brightness_state_save(brightness);
	return true;
}

bool wsm_brightness_restore(struct wsm_brightness *brightness) {
	if (!brightness || !brightness->impl->set_brightness ||
			!brightness->state_key) {
		return false;
	}
	struct brightness_state_entry *entries = brightness_state_load();
	struct brightness_state_entry *entry = entries;
	while (entry && strcmp(entry->key, brightness->state_key) != 0) {
		entry = entry->next;
	}
	if (!entry) {
		brightness_state_free(entries);
		return false;
	}
	long value = entry->value;
	brightness_state_free(entries);
	if (value < brightness->min_brightness) {
		value = brightness->min_brightness;
	} else if (value > brightness->max_brightness) {
		value = brightness->max_brightness;
	}
	if (!brightness->impl->set_brightness(brightness, value)) {
		return false;
	}
	brightness->brightness = value;
	return true;
}

const char *wsm_brightness_method_name(enum wsm_brightness_method method) {
	switch (method) {
	case WSM_BRIGHTNESS_METHOD_NONE: return "none";
	case WSM_BRIGHTNESS_METHOD_BACKLIGHT: return "backlight";
	case WSM_BRIGHTNESS_METHOD_DDCUTIL: return "ddcutil";
	}
	return "none";
}
