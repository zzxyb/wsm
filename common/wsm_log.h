#ifndef WSM_LOG_H
#define WSM_LOG_H

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

struct timespec;

typedef enum {
	WSM_SILENT = 0,
	WSM_ERROR = 1,
	WSM_INFO = 2,
	WSM_DEBUG = 3,
	WSM_LOG_IMPORTANCE_LAST,
} wsm_log_importance_t;

#ifdef __GNUC__
#define ATTRIB_PRINTF(start, end) __attribute__((format(printf, start, end)))
#else
#define ATTRIB_PRINTF(start, end)
#endif

void error_handler(int sig);

typedef void (*terminate_callback_t)(int exit_code);

void wsm_log_init(wsm_log_importance_t verbosity,
	terminate_callback_t terminate);
void timespec_sub(struct timespec *r, const struct timespec *a,
	const struct timespec *b);
void _wsm_log(wsm_log_importance_t verbosity,
	const char *format, ...) ATTRIB_PRINTF(2, 3);
void _wsm_vlog(wsm_log_importance_t verbosity,
	const char *format, va_list args) ATTRIB_PRINTF(2, 0);
void _wsm_abort(const char *filename, ...) ATTRIB_PRINTF(1, 2);
bool _wsm_assert(bool condition, const char* format, ...) ATTRIB_PRINTF(2, 3);

#ifdef WSM_REL_SRC_DIR
// strip prefix from __FILE__, leaving the path relative to the project root
#define _WSM_FILENAME ((const char *)__FILE__ + sizeof(WSM_REL_SRC_DIR) - 1)
#else
#define _WSM_FILENAME __FILE__
#endif

#define wsm_log(verb, fmt, ...) \
_wsm_log(verb, "[%s:%d] " fmt, _WSM_FILENAME, __LINE__, ##__VA_ARGS__)

#define wsm_vlog(verb, fmt, args) \
_wsm_vlog(verb, "[%s:%d] " fmt, _WSM_FILENAME, __LINE__, args)

#define wsm_log_errno(verb, fmt, ...) \
wsm_log(verb, fmt ": %s", ##__VA_ARGS__, strerror(errno))

#define wsm_abort(FMT, ...) \
_wsm_abort("[%s:%d] " FMT, _WSM_FILENAME, __LINE__, ##__VA_ARGS__)

#define wsm_assert(COND, FMT, ...) \
_wsm_assert(COND, "[%s:%d] %s:" FMT, _WSM_FILENAME, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)

#endif
		
