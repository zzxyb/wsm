#include "util/wsm_log.h"
#include "util/wsm_common.h"

#include <time.h>
#include <ctype.h>
#include <string.h>

static const char whitespace[] = " \f\n\r\t\v";

void strip_whitespace(char *str) {
	size_t len = strlen(str);
	size_t start = strspn(str, whitespace);
	memmove(str, &str[start], len + 1 - start);

	if (*str) {
		for (len -= start + 1; isspace(str[len]); --len) {}
		str[len + 1] = '\0';
	}
}

char *vformat_str(const char *fmt, va_list args) {
	char *str = NULL;
	va_list args_copy;
	va_copy(args_copy, args);

	int len = vsnprintf(NULL, 0, fmt, args);
	if (len < 0) {
		wsm_log_errno(WSM_ERROR, "vsnprintf(\"%s\") failed", fmt);
		goto out;
	}

	str = malloc(len + 1);
	if (str == NULL) {
		wsm_log_errno(WSM_ERROR, "malloc() failed");
		goto out;
	}

	vsnprintf(str, len + 1, fmt, args_copy);

out:
	va_end(args_copy);
	return str;
}

char *format_str(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char *str = vformat_str(fmt, args);
	va_end(args);
	return str;
}

char *lenient_strcat(char *dest, const char *src) {
	if (dest && src) {
		return strcat(dest, src);
	}

	return dest;
}

char *lenient_strncat(char *dest, const char *src, size_t len) {
	if (dest && src) {
		return strncat(dest, src, len);
	}

	return dest;
}

int lenient_strcmp(const char *a, const char *b) {
	if (a == b) {
		return 0;
	} else if (!a) {
		return -1;
	} else if (!b) {
		return 1;
	} else {
		return strcmp(a, b);
	}
}

void color_to_rgba(float dest[static 4], uint32_t color) {
	dest[0] = ((color >> 24) & 0xff) / 255.0;
	dest[1] = ((color >> 16) & 0xff) / 255.0;
	dest[2] = ((color >> 8) & 0xff) / 255.0;
	dest[3] = (color & 0xff) / 255.0;
}

char* int_to_string(int num) {
	int length = snprintf(NULL, 0, "%d", num);
	char *str = (char *)malloc((length + 1) * sizeof(char));
	if (str == NULL) {
		return NULL;
	}
	snprintf(str, length + 1, "%d", num);

	return str;
}

bool ends_with_str(const char *src, const char * dst) {
	size_t str_len = strlen(src);
	size_t suffix_len = strlen(dst);
	if (str_len < suffix_len) {
		return false;
	}

	return strcmp(src + str_len - suffix_len, dst) == 0;
}

void array_remove_at(struct wl_array *arr, size_t offset, size_t size) {
	assert(arr->size >= offset + size);

	char *data = arr->data;
	memmove(&data[offset], &data[offset + size], arr->size - offset - size);
	arr->size -= size;
}

bool array_realloc(struct wl_array *arr, size_t size) {
	// If the size is less than 1/4th of the allocation size, we shrink it.
	// 1/4th is picked to provide hysteresis, without which an array with size
	// arr->alloc would constantly reallocate if an element is added and then
	// removed continuously.
	size_t alloc;
	if (arr->alloc > 0 && size > arr->alloc / 4) {
		alloc = arr->alloc;
	} else {
		alloc = 16;
	}

	while (alloc < size) {
		alloc *= 2;
	}

	if (alloc == arr->alloc) {
		return true;
	}

	void *data = realloc(arr->data, alloc);
	if (data == NULL) {
		return false;
	}
	arr->data = data;
	arr->alloc = alloc;
	return true;
}
