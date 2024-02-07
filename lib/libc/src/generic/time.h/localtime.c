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