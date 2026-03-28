#include <unistd.h>
#include "__getpid.h"

extern pid_t __pid_cached;

pid_t getpid(void)
{
    if (__pid_cached != 0)
        return __pid_cached;

    return __getpid(GETPID_PID);
}