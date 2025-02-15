#include <stdlib.h>
#include <stdio.h>

char *getcwd(char buf[], size_t size)
{
    fprintf(stderr, "getcwd() is not implemented, returning \"/\"\n");
    // Just return the root directory for now
    if (size > 1) {
        buf[0] = '/';
        buf[1] = '\0';
    }
    return buf;
}