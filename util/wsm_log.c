#include "util/wsm_log.h"
#include "util/wsm_time.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static terminate_callback_t log_terminate = exit;

void _wsm_abort(const char *format, ...) {
	va_list args;
	va_start(args, format);
	_wsm_vlog(WSM_ERROR, format, args);
	va_end(args);
	log_terminate(EXIT_FAILURE);
}

bool _wsm_assert(bool condition, const char *format, ...) {
	if (condition) {
		return true;
	}

	va_list args;
	va_start(args, format);
	_wsm_vlog(WSM_ERROR, format, args);
	va_end(args);

#ifndef NDEBUG
	raise(SIGABRT);
#endif

	return false;
}

static bool colored = true;
static struct timespec start_time = {-1, -1};
static enum wsm_log_importance log_importance = WSM_ERROR;

static const char *verbosity_colors[] = {
	[WSM_SILENT] = "",
	[WSM_ERROR ] = "\x1B[1;31m",
	[WSM_INFO  ] = "\x1B[1;34m",
	[WSM_DEBUG ] = "\x1B[1;90m",
};

static const char *verbosity_headers[] = {
	[WSM_SILENT] = "",
	[WSM_ERROR] = "[ERROR]",
	[WSM_INFO] = "[INFO]",
	[WSM_DEBUG] = "[DEBUG]",
};

static void init_start_time(void) {
	if (start_time.tv_sec >= 0) {
		return;
	}
	clock_gettime(CLOCK_MONOTONIC, &start_time);
}

static void wsm_log_stderr(enum wsm_log_importance verbosity, const char *fmt,
		va_list args) {
	init_start_time();

	if (verbosity > log_importance) {
		return;
	}

	struct timespec ts = {0};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	timespec_sub(&ts, &ts, &start_time);

	fprintf(stderr, "%02d:%02d:%02d.%03ld ", (int)(ts.tv_sec / 60 / 60),
		(int)(ts.tv_sec / 60 % 60), (int)(ts.tv_sec % 60), ts.tv_nsec / 1000000);

	unsigned c = (verbosity < WSM_LOG_IMPORTANCE_LAST) ? verbosity :
		WSM_LOG_IMPORTANCE_LAST - 1;

	if (colored && isatty(STDERR_FILENO)) {
		fprintf(stderr, "%s", verbosity_colors[c]);
	} else {
		fprintf(stderr, "%s ", verbosity_headers[c]);
	}

	vfprintf(stderr, fmt, args);

	if (colored && isatty(STDERR_FILENO)) {
		fprintf(stderr, "\x1B[0m");
	}
	fprintf(stderr, "\n");
}

void wsm_log_init(enum wsm_log_importance verbosity, terminate_callback_t callback) {
	init_start_time();

	if (verbosity < WSM_LOG_IMPORTANCE_LAST) {
		log_importance = verbosity;
	}

	if (callback) {
		log_terminate = callback;
	}
}

enum wsm_log_importance wsm_log_get_verbosity(void) {
	return log_importance;
}

void _wsm_vlog(enum wsm_log_importance verbosity, const char *fmt, va_list args) {
	wsm_log_stderr(verbosity, fmt, args);
}

void _wsm_log(enum wsm_log_importance verbosity, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	wsm_log_stderr(verbosity, fmt, args);
	va_end(args);
}
