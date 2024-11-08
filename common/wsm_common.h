#ifndef WSM_COMMON_H
#define WSM_COMMON_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include <wayland-client-protocol.h>

struct timespec;

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
#endif

#ifdef __GNUC__
#define _WSM_ATTRIB_PRINTF(start, end) __attribute__((format(printf, start, end)))
#else
#define _WSM_ATTRIB_PRINTF(start, end)
#endif

/**
 * @brief Connects a signal handler function to a wl_signal.
 *
 * @param src Signal emitter (struct containing wl_signal)
 * @param dest Signal receiver (struct containing wl_listener)
 * @param name Signal name
 *
 * This assumes that the common pattern is followed where:
 *   - the wl_signal is (*src).events.<name>
 *   - the wl_listener is (*dest).<name>
 *   - the signal handler function is named handle_<name>
 */
#define WL_CONNECT_SIGNAL(src, dest, name) \
(dest)->name.notify = handle_##name; \
		wl_signal_add(&(src)->events.name, &(dest)->name)

/**
 * @brief Returns the minimum of two values.
 *
 * @note Arguments may be evaluated twice.
 */
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/**
 * @brief Returns the maximum of two values.
 *
 * @note Arguments may be evaluated twice.
 */
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

/**
 * @brief Avoids "unused parameter" warnings.
 * @param x The parameter to be ignored.
 */
#define W_UNUSED(x) (void)x;

/**
 * @brief Converts a string to an integer safely.
 *
 * Parses a base-10 number from the given string. Checks that the
 * string is not blank, contains only numerical characters, and is
 * within the range of INT32_MIN to INT32_MAX. If the validation is
 * successful, the result is stored in *value; otherwise *value is
 * unchanged and errno is set appropriately.
 *
 * @param str Input string to parse.
 * @param value Pointer to store the converted integer.
 * @return true if the number was parsed successfully, false on error.
 */
static inline bool safe_strtoint(const char *str, int32_t *value) {
	long ret;
	char *end;

	assert(str != NULL);

	errno = 0;
	ret = strtol(str, &end, 10);
	if (errno != 0) {
		return false;
	} else if (end == str || *end != '\0') {
		errno = EINVAL;
		return false;
	}

	if ((long)((int32_t)ret) != ret) {
		errno = ERANGE;
		return false;
	}
	*value = (int32_t)ret;

	return true;
}

/**
 * @brief Returns "yes" or "no" based on a boolean condition.
 * @param cond The condition to evaluate.
 * @return "yes" if true, "no" if false.
 */
static inline const char *yesno(bool cond) {
	return cond ? "yes" : "no";
}

/**
 * @brief Strips whitespace from a string.
 * @param str Pointer to the string to modify.
 */
void strip_whitespace(char *str);

/**
 * @brief Formats a string using a variable argument list.
 * @param fmt Format string.
 * @param args Variable argument list.
 * @return Pointer to the formatted string.
 */
char *vformat_str(const char *fmt, va_list args) _WSM_ATTRIB_PRINTF(1, 0);

/**
 * @brief Formats a string using a variable number of arguments.
 * @param fmt Format string.
 * @return Pointer to the formatted string.
 */
char *format_str(const char *fmt, ...) _WSM_ATTRIB_PRINTF(1, 2);

/**
 * @brief Gets the current time in milliseconds.
 * @return Current time in milliseconds.
 */
uint32_t get_current_time_msec();

/**
 * @brief Concatenates two strings leniently.
 * @param dest Destination string.
 * @param src Source string.
 * @return Pointer to the concatenated string.
 */
char *lenient_strcat(char *dest, const char *src);

/**
 * @brief Concatenates a specified number of characters from a source string to a destination string leniently.
 * @param dest Destination string.
 * @param src Source string.
 * @param len Number of characters to concatenate.
 * @return Pointer to the concatenated string.
 */
char *lenient_strncat(char *dest, const char *src, size_t len);

/**
 * @brief Compares two strings leniently.
 * @param a First string.
 * @param b Second string.
 * @return Integer less than, equal to, or greater than zero if a is found, respectively, to be less than, to match, or be greater than b.
 */
int lenient_strcmp(const char *a, const char *b);

/**
 * @brief Converts a color to RGBA format.
 * @param dest Array to store the RGBA values.
 * @param color The color value in ARGB format.
 */
void color_to_rgba(float dest[static 4], uint32_t color);

/**
 * @brief Converts an integer to a string.
 * @param num The integer to convert.
 * @return Pointer to the string representation of the integer.
 */
char* int_to_string(int num);

/**
 * @brief Converts a timespec structure to milliseconds.
 * @param a Pointer to the timespec structure.
 * @return Time in milliseconds.
 */
int64_t timespec_to_msec(const struct timespec *a);

/**
 * @brief Converts a timespec structure to nanoseconds.
 * @param a Pointer to the timespec structure.
 * @return Time in nanoseconds.
 */
int64_t timespec_to_nsec(const struct timespec *a);

/**
 * @brief Checks if a string ends with another string.
 * @param src Source string.
 * @param dst Destination string to check against.
 * @return true if src ends with dst, false otherwise.
 */
bool ends_with_str(const char *src, const char * dst);

#endif
