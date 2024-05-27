#include <sys/stat.h>

mode_t _umask = 022;

mode_t umask(mode_t mask)
{
    mode_t old = _umask;
    _umask = mask & 0777;
    return old;
}