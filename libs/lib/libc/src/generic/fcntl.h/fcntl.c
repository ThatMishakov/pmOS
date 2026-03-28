#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

int  fcntl(int fildes, int cmd, ...)
{
    fprintf(stderr, "pmOS libC: fcntl filedes %i cmd %i: fcntl not implemented!\n", fildes, cmd);
    errno = ENOSYS;
    return -1;
}