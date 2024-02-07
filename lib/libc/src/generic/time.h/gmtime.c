#include <time.h>
#include <errno.h>

#include "unix_to_tm.h"

static struct tm gmtime_tm = {};

struct tm * gmtime(const time_t *timer) {
    bool isdst = false;

    int result = __unix_time_to_tm(*timer, &gmtime_tm, isdst);
    if (result != 0) {
        // __convert_time() sets errno
        return NULL;
    }

    return &gmtime_tm;
}

struct tm * gmtime_r(const time_t *timer, struct tm *result) {
    if (result == NULL || timer == NULL) {
        errno = EINVAL;
        return NULL;
    }

    bool isdst = false;

    int r = __unix_time_to_tm(*timer, result, isdst);
    if (r != 0) {
        // __convert_time() sets errno
        return NULL;
    }

    return result;
}