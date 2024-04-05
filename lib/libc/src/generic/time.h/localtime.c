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

#include <time.h>
#include <errno.h>

#include "unix_to_tm.h"

static struct tm localtime_tm = {};

long __get_local_offset(void) {
    // TODO: Imlement
    return 0;
}

struct tm *localtime(const time_t *timer) {
    long local_offset = __get_local_offset();
    bool isdst = false;

    time_t local_time = *timer + local_offset;
    // Check for overflow
    if (local_offset > 0 && local_time < *timer) {
        errno = EOVERFLOW;
        return NULL;
    } else if (local_offset < 0 && local_time > *timer) {
        errno = EOVERFLOW;
        return NULL;
    }

    int result = __unix_time_to_tm(local_time, &localtime_tm, isdst);
    if (result != 0) {
        // __convert_time() sets errno
        return NULL;
    }

    return &localtime_tm;
}

struct tm *localtime_r(const time_t *timer, struct tm *result) {
    if (result == NULL || timer == NULL) {
        errno = EINVAL;
        return NULL;
    }

    long local_offset = __get_local_offset();
    bool isdst = false;

    time_t local_time = *timer + local_offset;
    // Check for overflow
    if (local_offset > 0 && local_time < *timer) {
        errno = EOVERFLOW;
        return NULL;
    } else if (local_offset < 0 && local_time > *timer) {
        errno = EOVERFLOW;
        return NULL;
    }

    int r = __unix_time_to_tm(*timer, result, isdst);
    if (r != 0) {
        // __convert_time() sets errno
        return NULL;
    }

    return result;
}