#include <errno.h>

int killpg(int pgrp, int sig) {
    errno = ENOSYS;
    return -1;
}