/**
 * @file        wsm_time.h
 * @brief       Time conversion and arithmetic helpers.
 * @details     This file provides millisecond and nanosecond conversion
 *              functions for POSIX timespec values.
 * @author      YaoBing Xiao
 * @date        2026-08-23
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-08-23, initial version\n
 */

#ifndef UTIL_WSM_TIME_H
#define UTIL_WSM_TIME_H

#include <stdint.h>
#include <time.h>

static const long NSEC_PER_SEC = 1000000000; /**< Nanoseconds in one second. */

/**
 * @brief Gets the current time in milliseconds.
 * @return Current wall-clock time in milliseconds.
 */
int64_t get_current_time_msec(void);

/**
 * @brief Converts a timespec to milliseconds.
 * @param a Timespec to convert.
 * @return Time value in milliseconds.
 */
int64_t timespec_to_msec(const struct timespec *a);

/**
 * @brief Converts a timespec to nanoseconds.
 * @param a Timespec to convert.
 * @return Time value in nanoseconds.
 */
int64_t timespec_to_nsec(const struct timespec *a);

/**
 * @brief Converts nanoseconds to a timespec.
 * @param r Destination timespec.
 * @param nsec Nanosecond value.
 */
void timespec_from_nsec(struct timespec *r, int64_t nsec);

/**
 * @brief Subtracts one timespec from another.
 * @param r Destination difference.
 * @param a Minuend timespec.
 * @param b Subtrahend timespec.
 */
void timespec_sub(struct timespec *r, const struct timespec *a,
		const struct timespec *b);

#endif // UTIL_WSM_TIME_H
