#include <time.h>

time_t __get_system_time(void)
{
    return 1690934400;
}

time_t time(time_t *tloc)
{
    time_t time = __get_system_time();

    if (tloc != NULL && time != (time_t) -1) {
        *tloc = time;
    }

    return time;
}