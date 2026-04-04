#include <unistd.h>
#include <stdio.h>
#include <errno.h>

int getgroups(int gidsetsize, gid_t grouplist[])
{
    (void)gidsetsize;
    (void)grouplist;
    
    fprintf(stderr, "pmOS libc: getgroups() is not implemented!\n");
    errno = ENOSYS;
    return -1;
}