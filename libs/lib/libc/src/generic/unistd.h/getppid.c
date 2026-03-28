#include <unistd.h>
#include "__getpid.h"

pid_t getppid(void)
{
    return __getpid(GETPID_PPID);
}