#include <unistd.h>
#include "__getpid.h"

pid_t getpgrp(void)
{
    return __getpid(GETPID_PGID);
}