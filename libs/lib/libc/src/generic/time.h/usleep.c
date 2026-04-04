#include <unistd.h>
#include <errno.h>
#include <time.h>

int usleep(useconds_t useconds)
{
    struct timespec ts;
    int res;

    ts.tv_sec = useconds / 1000000;
    ts.tv_nsec = (useconds % 1000000) * 1000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}