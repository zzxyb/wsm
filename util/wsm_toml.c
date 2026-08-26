#include "util/wsm_toml.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void set_error(char *error, size_t size, const char *format, ...) {
	if (error == NULL || size == 0) {
		return;
	}
	va_list args;
	va_start(args, format);
	vsnprintf(error, size, format, args);
	va_end(args);
}

static char *trim(char *str) {
	while (isspace((unsigned char)*str)) {
		str++;
	}
	char *end = str + strlen(str);
	while (end > str && isspace((unsigned char)end[-1])) {
		end--;
	}
	*end = '\0';
	return str;
}

static bool valid_key(const char *key) {
	if (key == NULL || *key == '\0') {
		errno = EINVAL;
		return false;
	}
	bool segment = false;
	for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
		if (*p == '.') {
			if (!segment || p[1] == '\0') {
				errno = EINVAL;
				return false;
			}
			segment = false;
		} else if (isalnum(*p) || *p == '_' || *p == '-') {
			segment = true;
		} else {
			errno = EINVAL;
			return false;
		}
	}
	return true;
}

const struct wsm_toml_entry *wsm_toml_get_entry(
	const struct wsm_toml *toml, const char *key) {
	if (toml == NULL || key == NULL) {
		return NULL;
	}
	struct wsm_toml_entry *entry;
	wl_list_for_each(entry, &toml->entries, link) {
		if (strcmp(entry->key, key) == 0) {
			return entry;
		}
	}
	return NULL;
}

static struct wsm_toml_entry *find_entry(
	struct wsm_toml *toml, const char *key) {
	return (struct wsm_toml_entry *)wsm_toml_get_entry(toml, key);
}

static void clear_value(struct wsm_toml_entry *entry) {
	if (entry->type == WSM_TOML_STRING) {
		free(entry->value.string);
	}
}

static struct wsm_toml_entry *get_or_create_entry(
	struct wsm_toml *toml, const char *key) {
	if (toml == NULL || !valid_key(key)) {
		errno = EINVAL;
		return NULL;
	}
	struct wsm_toml_entry *entry = find_entry(toml, key);
	if (entry != NULL) {
		clear_value(entry);
		return entry;
	}
	entry = calloc(1, sizeof(*entry));
	if (entry == NULL) {
		return NULL;
	}
	entry->key = strdup(key);
	if (entry->key == NULL) {
		free(entry);
		return NULL;
	}
	wl_list_insert(toml->entries.prev, &entry->link);
	return entry;
}

struct wsm_toml *wsm_toml_create(void) {
	struct wsm_toml *toml = calloc(1, sizeof(*toml));
	if (toml != NULL) {
		wl_list_init(&toml->entries);
	}
	return toml;
}

void wsm_toml_destroy(struct wsm_toml *toml) {
	if (toml == NULL) {
		return;
	}
	struct wsm_toml_entry *entry, *tmp;
	wl_list_for_each_safe(entry, tmp, &toml->entries, link) {
		clear_value(entry);
		free(entry->key);
		free(entry);
	}
	free(toml);
}

bool wsm_toml_has(const struct wsm_toml *toml, const char *key) {
	return wsm_toml_get_entry(toml, key) != NULL;
}

static bool get_entry(const struct wsm_toml *toml, const char *key,
	enum wsm_toml_type type, const struct wsm_toml_entry **result) {
	if (toml == NULL || key == NULL || result == NULL) {
		errno = EINVAL;
		return false;
	}
	const struct wsm_toml_entry *entry = wsm_toml_get_entry(toml, key);
	if (entry == NULL) {
		errno = ENOENT;
		return false;
	}
	if (entry->type != type) {
		errno = EINVAL;
		return false;
	}
	*result = entry;
	return true;
}

bool wsm_toml_get_string(
	const struct wsm_toml *toml, const char *key, char **value) {
	const struct wsm_toml_entry *entry;
	if (value == NULL) {
		errno = EINVAL;
		return false;
	}
	if (!get_entry(toml, key, WSM_TOML_STRING, &entry)) {
		return false;
	}
	*value = strdup(entry->value.string);
	return *value != NULL;
}

bool wsm_toml_get_int(
	const struct wsm_toml *toml, const char *key, int64_t *value) {
	const struct wsm_toml_entry *entry;
	if (value == NULL) {
		errno = EINVAL;
		return false;
	}
	if (!get_entry(toml, key, WSM_TOML_INTEGER, &entry)) {
		return false;
	}
	*value = entry->value.integer;
	return true;
}

bool wsm_toml_get_double(
	const struct wsm_toml *toml, const char *key, double *value) {
	const struct wsm_toml_entry *entry;
	if (value == NULL) {
		errno = EINVAL;
		return false;
	}
	if (!get_entry(toml, key, WSM_TOML_DOUBLE, &entry)) {
		return false;
	}
	*value = entry->value.decimal;
	return true;
}

bool wsm_toml_get_bool(
	const struct wsm_toml *toml, const char *key, bool *value) {
	const struct wsm_toml_entry *entry;
	if (value == NULL) {
		errno = EINVAL;
		return false;
	}
	if (!get_entry(toml, key, WSM_TOML_BOOLEAN, &entry)) {
		return false;
	}
	*value = entry->value.boolean;
	return true;
}

bool wsm_toml_set_string(
	struct wsm_toml *toml, const char *key, const char *value) {
	if (value == NULL) {
		errno = EINVAL;
		return false;
	}
	char *copy = strdup(value);
	if (copy == NULL) {
		return false;
	}
	struct wsm_toml_entry *entry = get_or_create_entry(toml, key);
	if (entry == NULL) {
		free(copy);
		return false;
	}
	entry->type = WSM_TOML_STRING;
	entry->value.string = copy;
	return true;
}

bool wsm_toml_set_int(struct wsm_toml *toml, const char *key, int64_t value) {
	struct wsm_toml_entry *entry = get_or_create_entry(toml, key);
	if (entry == NULL) {
		return false;
	}
	entry->type = WSM_TOML_INTEGER;
	entry->value.integer = value;
	return true;
}

bool wsm_toml_set_double(struct wsm_toml *toml, const char *key, double value) {
	struct wsm_toml_entry *entry = get_or_create_entry(toml, key);
	if (entry == NULL) {
		return false;
	}
	entry->type = WSM_TOML_DOUBLE;
	entry->value.decimal = value;
	return true;
}

bool wsm_toml_set_bool(struct wsm_toml *toml, const char *key, bool value) {
	struct wsm_toml_entry *entry = get_or_create_entry(toml, key);
	if (entry == NULL) {
		return false;
	}
	entry->type = WSM_TOML_BOOLEAN;
	entry->value.boolean = value;
	return true;
}

bool wsm_toml_remove(struct wsm_toml *toml, const char *key) {
	if (toml == NULL || key == NULL) {
		errno = EINVAL;
		return false;
	}
	struct wsm_toml_entry *entry = find_entry(toml, key);
	if (entry == NULL) {
		errno = ENOENT;
		return false;
	}
	wl_list_remove(&entry->link);
	clear_value(entry);
	free(entry->key);
	free(entry);
	return true;
}

static char *parse_string(const char *raw) {
	size_t length = strlen(raw);
	if (length < 2 || raw[0] != '"' || raw[length - 1] != '"') {
		errno = EINVAL;
		return NULL;
	}
	char *value = malloc(length);
	if (value == NULL) {
		return NULL;
	}
	size_t out = 0;
	for (size_t i = 1; i + 1 < length; i++) {
		unsigned char ch = raw[i];
		if (ch != '\\') {
			value[out++] = ch;
			continue;
		}
		if (++i + 1 >= length) {
			free(value);
			errno = EINVAL;
			return NULL;
		}
		switch (raw[i]) {
		case 'b':
			value[out++] = '\b';
			break;
		case 't':
			value[out++] = '\t';
			break;
		case 'n':
			value[out++] = '\n';
			break;
		case 'f':
			value[out++] = '\f';
			break;
		case 'r':
			value[out++] = '\r';
			break;
		case '"':
			value[out++] = '"';
			break;
		case '\\':
			value[out++] = '\\';
			break;
		default:
			free(value);
			errno = EINVAL;
			return NULL;
		}
	}
	value[out] = '\0';
	return value;
}

static bool parse_value(struct wsm_toml *toml, const char *key, char *raw) {
	if (*raw == '"') {
		char *value = parse_string(raw);
		if (value == NULL) {
			return false;
		}
		bool ok = wsm_toml_set_string(toml, key, value);
		free(value);
		return ok;
	}
	if (strcmp(raw, "true") == 0 || strcmp(raw, "false") == 0) {
		return wsm_toml_set_bool(toml, key, raw[0] == 't');
	}
	char *end;
	errno = 0;
	int64_t integer = strtoimax(raw, &end, 0);
	if (errno == 0 && end != raw && *end == '\0') {
		return wsm_toml_set_int(toml, key, integer);
	}
	errno = 0;
	double decimal = strtod(raw, &end);
	if (errno == 0 && end != raw && *end == '\0') {
		return wsm_toml_set_double(toml, key, decimal);
	}
	errno = EINVAL;
	return false;
}

struct wsm_toml *wsm_toml_load(
	const char *path, char *error, size_t error_size) {
	if (path == NULL) {
		errno = EINVAL;
		return NULL;
	}
	FILE *file = fopen(path, "r");
	if (file == NULL) {
		set_error(error, error_size, "%s", strerror(errno));
		return NULL;
	}
	struct wsm_toml *toml = wsm_toml_create();
	if (toml == NULL) {
		fclose(file);
		return NULL;
	}
	char *line = NULL;
	size_t capacity = 0;
	char table[512] = {0};
	unsigned long line_number = 0;
	while (getline(&line, &capacity, file) >= 0) {
		line_number++;
		bool quoted = false, escaped = false;
		for (char *p = line; *p; p++) {
			if (escaped) {
				escaped = false;
			} else if (quoted && *p == '\\') {
				escaped = true;
			} else if (*p == '"') {
				quoted = !quoted;
			} else if (!quoted && *p == '#') {
				*p = '\0';
				break;
			}
		}
		char *text = trim(line);
		if (*text == '\0') {
			continue;
		}
		if (*text == '[') {
			size_t length = strlen(text);
			if (length < 3 || text[length - 1] != ']') {
				goto syntax_error;
			}
			text[length - 1] = '\0';
			text = trim(text + 1);
			if (!valid_key(text) || strlen(text) >= sizeof(table)) {
				goto syntax_error;
			}
			strcpy(table, text);
			continue;
		}
		quoted = escaped = false;
		char *equal = NULL;
		for (char *p = text; *p; p++) {
			if (escaped) {
				escaped = false;
			} else if (quoted && *p == '\\') {
				escaped = true;
			} else if (*p == '"') {
				quoted = !quoted;
			} else if (!quoted && *p == '=') {
				equal = p;
				break;
			}
		}
		if (equal == NULL) {
			goto syntax_error;
		}
		*equal = '\0';
		char *name = trim(text);
		char *raw = trim(equal + 1);
		char key[1024];
		int length = table[0]
			? snprintf(key, sizeof(key), "%s.%s", table, name)
			: snprintf(key, sizeof(key), "%s", name);
		if (length < 0 || (size_t)length >= sizeof(key) ||
			!valid_key(key) ||
			wsm_toml_get_entry(toml, key) != NULL ||
			!parse_value(toml, key, raw)) {
			goto syntax_error;
		}
	}
	free(line);
	if (ferror(file)) {
		set_error(error, error_size, "%s", strerror(errno));
		fclose(file);
		wsm_toml_destroy(toml);
		return NULL;
	}
	fclose(file);
	return toml;

syntax_error:
	set_error(error, error_size, "line %lu: invalid TOML", line_number);
	free(line);
	fclose(file);
	wsm_toml_destroy(toml);
	errno = EINVAL;
	return NULL;
}

static bool write_string(FILE *file, const char *value) {
	if (fputc('"', file) == EOF) {
		return false;
	}
	for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
		switch (*p) {
		case '\b':
			if (fputs("\\b", file) == EOF)
				return false;
			break;
		case '\t':
			if (fputs("\\t", file) == EOF)
				return false;
			break;
		case '\n':
			if (fputs("\\n", file) == EOF)
				return false;
			break;
		case '\f':
			if (fputs("\\f", file) == EOF)
				return false;
			break;
		case '\r':
			if (fputs("\\r", file) == EOF)
				return false;
			break;
		case '"':
			if (fputs("\\\"", file) == EOF)
				return false;
			break;
		case '\\':
			if (fputs("\\\\", file) == EOF)
				return false;
			break;
		default:
			if (fputc(*p, file) == EOF)
				return false;
			break;
		}
	}
	return fputc('"', file) != EOF;
}

static bool write_entry(
	FILE *file, const struct wsm_toml_entry *entry, const char *name) {
	if (fprintf(file, "%s = ", name) < 0) {
		return false;
	}
	switch (entry->type) {
	case WSM_TOML_STRING:
		if (!write_string(file, entry->value.string))
			return false;
		break;
	case WSM_TOML_INTEGER:
		if (fprintf(file, "%" PRId64, entry->value.integer) < 0)
			return false;
		break;
	case WSM_TOML_DOUBLE:
		if (isfinite(entry->value.decimal) &&
			entry->value.decimal == trunc(entry->value.decimal)) {
			if (fprintf(file, "%.1f", entry->value.decimal) < 0)
				return false;
		} else if (fprintf(file, "%.15g", entry->value.decimal) < 0) {
			return false;
		}
		break;
	case WSM_TOML_BOOLEAN:
		if (fputs(entry->value.boolean ? "true" : "false", file) == EOF)
			return false;
		break;
	}
	return fputc('\n', file) != EOF;
}

static int compare_entries(const void *left, const void *right) {
	const struct wsm_toml_entry *a =
		*(const struct wsm_toml_entry *const *)left;
	const struct wsm_toml_entry *b =
		*(const struct wsm_toml_entry *const *)right;
	return strcmp(a->key, b->key);
}

static bool write_document(FILE *file, const struct wsm_toml *toml) {
	size_t count = wl_list_length(&toml->entries);
	struct wsm_toml_entry **entries = calloc(count, sizeof(*entries));
	if (entries == NULL && count > 0) {
		return false;
	}
	size_t index = 0;
	struct wsm_toml_entry *entry;
	wl_list_for_each(entry, &toml->entries, link) {
		entries[index++] = entry;
	}
	qsort(entries, count, sizeof(*entries), compare_entries);

	for (index = 0; index < count; index++) {
		entry = entries[index];
		if (strchr(entry->key, '.') == NULL &&
			!write_entry(file, entry, entry->key)) {
			free(entries);
			return false;
		}
	}
	char last_table[1024] = {0};
	for (index = 0; index < count; index++) {
		entry = entries[index];
		const char *dot = strrchr(entry->key, '.');
		if (dot == NULL) {
			continue;
		}
		size_t length = dot - entry->key;
		if (strlen(last_table) != length ||
			strncmp(last_table, entry->key, length) != 0) {
			if (length >= sizeof(last_table)) {
				errno = EOVERFLOW;
				free(entries);
				return false;
			}
			memcpy(last_table, entry->key, length);
			last_table[length] = '\0';
			if (fprintf(file, "\n[%s]\n", last_table) < 0) {
				free(entries);
				return false;
			}
		}
		if (!write_entry(file, entry, dot + 1)) {
			free(entries);
			return false;
		}
	}
	free(entries);
	return true;
}

bool wsm_toml_save(const struct wsm_toml *toml, const char *path) {
	if (toml == NULL || path == NULL) {
		errno = EINVAL;
		return false;
	}
	size_t length = strlen(path);
	char *temporary = malloc(length + sizeof(".tmp.XXXXXX"));
	if (temporary == NULL) {
		return false;
	}
	sprintf(temporary, "%s.tmp.XXXXXX", path);
	int fd = mkstemp(temporary);
	if (fd < 0) {
		free(temporary);
		return false;
	}
	FILE *file = fdopen(fd, "w");
	if (file == NULL) {
		int saved = errno;
		close(fd);
		unlink(temporary);
		free(temporary);
		errno = saved;
		return false;
	}
	bool ok = write_document(file, toml) && fflush(file) == 0 &&
		fsync(fd) == 0;
	int saved = errno;
	if (fclose(file) != 0) {
		ok = false;
		saved = errno;
	}
	if (ok && rename(temporary, path) != 0) {
		ok = false;
		saved = errno;
	}
	if (!ok) {
		unlink(temporary);
	}
	free(temporary);
	errno = saved;
	return ok;
}
