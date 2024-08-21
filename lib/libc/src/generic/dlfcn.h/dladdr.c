#include <dlfcn.h>
#include <stdio.h>
#include <errno.h>

int dladdr(const void *addr, Dl_info *info)
{
    fprintf(stderr, "pmOS libc: dladdr is not implemented\n");
    errno = ENOSYS;
    return -1;
}