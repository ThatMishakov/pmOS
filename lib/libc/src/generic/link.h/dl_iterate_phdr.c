#include <link.h>
#include <stdio.h>
#include <errno.h>

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data)
{
    fprintf(stderr, "pmOS libc: dl_iterate_phdr is not implemented\n");
    errno = ENOSYS;
    return -1;
}