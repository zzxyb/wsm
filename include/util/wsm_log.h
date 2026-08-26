/**
 * @file        wsm_log.h
 * @brief       Logging, assertion, and abort helpers for wlframe.
 * @details     This file defines log importance levels, printf-checked logging
 *              functions, file/line aware logging macros, assertion helpers,
 *              abort helpers, and small time utility declarations used by the
 *              logging system.
 * @author      YaoBing Xiao
 * @date        2026-07-21
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-07-21, initial version\n
 */

#ifndef UTIL_WSM_LOG_H
#define UTIL_WSM_LOG_H

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

struct timespec;

/**
 * @brief Enumeration of log importance levels.
 */
enum wsm_log_importance {
	WSM_SILENT = 0, /**< No logging */
	WSM_ERROR = 1, /**< Error messages */
	WSM_INFO = 2, /**< Informational messages */
	WSM_DEBUG = 3, /**< Debugging messages */
	WSM_LOG_IMPORTANCE_LAST, /**< Marker for the last log importance level */
};

#ifdef __GNUC__
/**
 * @brief Macro to specify printf-style format attributes for functions.
 * @param start The index of the first variable argument.
 * @param end The index of the last variable argument.
 */
#define ATTRIB_PRINTF(start, end) __attribute__((format(printf, start, end)))
#else
/** @brief No format attribute is available on non-GNU compilers. */
#define ATTRIB_PRINTF(start, end)
#endif

/**
 * @brief Signal handler for error handling.
 * @param sig The signal number.
 */
void error_handler(int sig);

/**
 * @brief Type definition for a termination callback function.
 * @param exit_code The exit code to use for termination.
 */
typedef void (*terminate_callback_t)(int exit_code);

/**
 * @brief Initializes the logging system.
 * @param verbosity The verbosity level for logging.
 * @param terminate Callback function to call on termination.
 */
void wsm_log_init(enum wsm_log_importance verbosity,
	terminate_callback_t terminate);

/**
 * @brief Gets the current logging verbosity.
 * @return Current logging importance threshold.
 */
enum wsm_log_importance wsm_log_get_verbosity(void);

/**
 * @brief Logs a message with a specified verbosity level.
 * @param verbosity The verbosity level for the log message.
 * @param format The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
void _wsm_log(enum wsm_log_importance verbosity,
	const char *format, ...) ATTRIB_PRINTF(2, 3);

/**
 * @brief Logs a message with a specified verbosity level using a va_list.
 * @param verbosity The verbosity level for the log message.
 * @param format The format string for the log message.
 * @param args The variable argument list.
 */
void _wsm_vlog(enum wsm_log_importance verbosity,
	const char *format, va_list args) ATTRIB_PRINTF(2, 0);

/**
 * @brief Aborts the program with a formatted message.
 * @param format The format string for the abort message.
 * @param ... Additional arguments for the format string.
 */
void _wsm_abort(const char *format, ...) ATTRIB_PRINTF(1, 2);

/**
 * @brief Asserts a condition and logs a message if the condition is false.
 * @param condition The condition to check.
 * @param format The format string for the log message.
 * @return true if the condition is true, false otherwise.
 */
bool _wsm_assert(bool condition, const char* format, ...) ATTRIB_PRINTF(2, 3);

#ifdef WSM_REL_SRC_DIR
/** @brief Strips the source-directory prefix from a compiler file name. */
#define _WSM_FILENAME ((const char *)__FILE__ + sizeof(WSM_REL_SRC_DIR) - 1)
#else
/** @brief Uses the compiler-provided full file name when no prefix is configured. */
#define _WSM_FILENAME __FILE__
#endif // WSM_REL_SRC_DIR

/**
 * @brief Macro for logging messages with file and line information.
 * @param verb The verbosity level for the log message.
 * @param fmt The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define wsm_log(verb, fmt, ...) \
_wsm_log(verb, "[%s:%d] " fmt, _WSM_FILENAME, __LINE__, ##__VA_ARGS__)

/**
 * @brief Macro for logging messages with a va_list and file/line information.
 * @param verb The verbosity level for the log message.
 * @param fmt The format string for the log message.
 * @param args The variable argument list.
 */
#define wsm_vlog(verb, fmt, args) \
_wsm_vlog(verb, "[%s:%d] " fmt, _WSM_FILENAME, __LINE__, args)

/**
 * @brief Macro for logging error messages with errno information.
 * @param verb The verbosity level for the log message.
 * @param fmt The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define wsm_log_errno(verb, fmt, ...) \
wsm_log(verb, fmt ": %s", ##__VA_ARGS__, strerror(errno))

/**
 * @brief Macro for aborting the program with a formatted message.
 * @param FMT The format string for the abort message.
 * @param ... Additional arguments for the format string.
 */
#define wsm_abort(FMT, ...) \
_wsm_abort("[%s:%d] " FMT, _WSM_FILENAME, __LINE__, ##__VA_ARGS__)

/**
 * @brief Macro for asserting a condition with a formatted message.
 * @param COND The condition to check.
 * @param FMT The format string for the log message.
 * @param ... Additional arguments for the format string.
 */
#define wsm_assert(COND, FMT, ...) \
_wsm_assert(COND, "[%s:%d] %s:" FMT, _WSM_FILENAME, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)

#endif // UTIL_WSM_LOG_H
