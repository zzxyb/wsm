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

#include "wsm_log.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static terminate_callback_t log_terminate = exit;
static const long NSEC_PER_SEC = 1000000000;

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
static wsm_log_importance_t log_importance = WSM_ERROR;

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

void timespec_sub(struct timespec *r, const struct timespec *a,
	const struct timespec *b) {
	r->tv_sec = a->tv_sec - b->tv_sec;
	r->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (r->tv_nsec < 0) {
		r->tv_sec--;
		r->tv_nsec += NSEC_PER_SEC;
	}
}

static void init_start_time(void) {
	if (start_time.tv_sec >= 0) {
		return;
	}
	clock_gettime(CLOCK_MONOTONIC, &start_time);
}

static void wsm_log_stderr(wsm_log_importance_t verbosity, const char *fmt,
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

void wsm_log_init(wsm_log_importance_t verbosity, terminate_callback_t callback) {
	init_start_time();

	if (verbosity < WSM_LOG_IMPORTANCE_LAST) {
		log_importance = verbosity;
	}

	if (callback) {
		log_terminate = callback;
	}
}

void _wsm_vlog(wsm_log_importance_t verbosity, const char *fmt, va_list args) {
	wsm_log_stderr(verbosity, fmt, args);
}

void _wsm_log(wsm_log_importance_t verbosity, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	wsm_log_stderr(verbosity, fmt, args);
	va_end(args);
}
