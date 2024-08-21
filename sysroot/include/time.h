/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __TIME_H
#define __TIME_H 1
#include <signal.h>

#define __DECLARE_CLOCK_T
#define __DECLARE_TIME_T
#define __DECLARE_SIZE_T
#define __DECLARE_PID_T
#define __DECLARE_CLOCKID_T
#define __DECLARE_TIMER_T
#define __DECLARE_LOCALE_T
#define __DECLARE_NULL
#define __DECLARE_TIMESPEC
#define __DECLARE_TIME_T
#define __DECLARE_CLOCK_T
#define __DECLARE_NULL
#include "__posix_types.h"

extern int daylight;
extern long timezone;
extern char *tzname[];

#if defined(__cplusplus)
extern "C" {
#endif

unsigned long pmos_clocks_per_sec(void);

/// @brief The number of clock ticks per second.
#define CLOCKS_PER_SEC pmos_clocks_per_sec()
// #define TIME_UTC 0

/// Monotonic system-wide clock.
#define CLOCK_MONOTONIC 0

/// The CPU-time clock of the calling process.
#define CLOCK_PROCESS_CPUTIME_ID 1

/// The system-wide clock that measures real (i.e., wall-clock) time.
#define CLOCK_REALTIME 2

/// The CPU-time clock of the calling thread.
#define CLOCK_THREAD_CPUTIME_ID 3

/// Flag indicating time is absolute.
#define TIMER_ABSTIME 1

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

#ifdef __STDC_HOSTED__
/* Time manipulation */
clock_t clock(void);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm *timeptr);

/**
 * @brief Get the current calendar time.
 *
 * The `time` function returns the current calendar time as the number of seconds
 * elapsed since the epoch (January 1, 1970, 00:00:00 UTC).
 *
 * @param timer Pointer to a `time_t` object where the current time will be stored.
 * @return The current calendar time as the number of seconds since the epoch. If an
 *         error occurred, the function returns `(time_t)-1`.
 */
time_t time(time_t *timer);

int timespec_get(struct timespec *ts, int base);

/* Time conversion */
char *asctime(const struct tm *timeptr);
char *asctime_r(const struct tm *, char *);
char *ctime(const time_t *timer);
char *ctime_r(const time_t *, char *);

/**
 * @brief Convert time_t to UTC time structure.
 *
 * The `gmtime` function converts the given time in seconds since the
 * Unix epoch (time_t) into a `struct tm` representing Coordinated
 * Universal Time (UTC).
 *
 * @param timeptr A pointer to a time_t containing the time value.
 * @return A pointer to a `struct tm` representing the UTC time.
 *         Returns NULL if an error occurs.
 */
struct tm *gmtime(const time_t *timeptr);

/**
 * @brief Convert time_t to UTC time structure (reentrant).
 *
 * The `gmtime_r` function is similar to `gmtime` but reentrant, allowing
 * multiple threads to safely use it simultaneously by providing a buffer
 * to store the result.
 *
 * @param timeptr A pointer to a time_t containing the time value.
 * @param result A pointer to a `struct tm` where the UTC time will be stored.
 * @return A pointer to the `struct tm` representing the UTC time.
 *         Returns NULL if an error occurs.
 */
struct tm *gmtime_r(const time_t *timeptr, struct tm *result);

/**
 * @brief Convert a time_t value to a broken-down time structure (thread-safe).
 *
 * The localtime_r() function converts the given time value pointed to by timep
 * into a broken-down time structure expressed as a local time. The resulting
 * structure is stored in the tm structure pointed to by result. The time is
 * represented in seconds since the epoch (00:00:00 UTC, January 1, 1970).
 *
 * @param timep Pointer to a time_t value representing the time to convert.
 * @param result Pointer to a struct tm where the result will be stored.
 * @return If successful, a pointer to the struct tm containing the local time.
 *         If an error occurs, NULL is returned, and errno is set accordingly.
 */
struct tm *localtime_r(const time_t *timep, struct tm *result);

/**
 * @brief Convert a time_t value to a broken-down time structure (not thread-safe).
 *
 * The localtime() function converts the given time value pointed to by timep
 * into a broken-down time structure expressed as a local time. The resulting
 * structure is stored in a static internal object. Since the internal object is
 * shared among all threads, this function is not thread-safe. To ensure
 * thread-safety, it is recommended to use localtime_r() instead.
 *
 * @param timep Pointer to a time_t value representing the time to convert.
 * @return If successful, a pointer to the struct tm containing the local time.
 *         If an error occurs, NULL is returned, and errno is set accordingly.
 */
struct tm *localtime(const time_t *timep);

size_t strftime(char *s, size_t maxsize, const char *format, const struct tm *timeptr);
size_t strftime_l(char *, size_t, const char *, const struct tm *, locale_t);

int clock_getres(clockid_t, struct timespec *);
int clock_gettime(clockid_t, struct timespec *);
int clock_nanosleep(clockid_t, int, const struct timespec *, struct timespec *);
int clock_settime(clockid_t, const struct timespec *);

int clock_getcpuclockid(pid_t, clockid_t *);

double difftime(time_t, time_t);
struct tm *getdate(const char *);

int nanosleep(const struct timespec *, struct timespec *);
char *strptime(const char *, const char *, struct tm *);

int timer_create(clockid_t, struct sigevent *, timer_t *);
int timer_delete(timer_t);
int timer_getoverrun(timer_t);
int timer_gettime(timer_t, struct itimerspec *);
int timer_settime(timer_t, int, const struct itimerspec *, struct itimerspec *);
void tzset(void);

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif