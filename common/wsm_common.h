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

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
#endif

#ifdef __GNUC__
#define _WSM_ATTRIB_PRINTF(start, end) __attribute__((format(printf, start, end)))
#else
#define _WSM_ATTRIB_PRINTF(start, end)
#endif

/**
 * @brief connect a signal handler function to a wl_signal.
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
 * @brief minimum of two values.
 *
 * @note Arguments may be evaluated twice.
 */
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/**
 * @brief maximum of two values.
 *
 * @note Arguments may be evaluated twice.
 */
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

/**
 * @brief safe_strtoint Convert string to integer
 * Parses a base-10 number from the given string.  Checks that the
 * string is not blank, contains only numerical characters, and is
 * within the range of INT32_MIN to INT32_MAX.  If the validation is
 * successful the result is stored in *value; otherwise *value is
 * unchanged and errno is set appropriately.
 * @param str input string
 * @param value Convert string to integer
 * @return return true if the number parsed successfully, false on error
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

static inline const char *yesno(bool cond) {
    return cond ? "yes" : "no";
}

void strip_whitespace(char *str);
char *vformat_str(const char *fmt, va_list args) _WSM_ATTRIB_PRINTF(1, 0);
char *format_str(const char *fmt, ...) _WSM_ATTRIB_PRINTF(1, 2);
uint32_t get_current_time_msec();
char *lenient_strcat(char *dest, const char *src);
char *lenient_strncat(char *dest, const char *src, size_t len);

#endif
