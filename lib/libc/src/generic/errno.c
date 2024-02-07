#include <errno.h>

int* __attribute__((weak)) __get_errno()
{
    static int __thread __errno = 0;
    return &__errno;
}