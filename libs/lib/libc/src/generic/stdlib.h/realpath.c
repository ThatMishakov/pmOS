#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *realpath(const char *restrict path, char *restrict resolved_path)
{
    fprintf(stderr, "realpath() is not implemented, returning the same path\n");
    // Just return the path for now

    if (resolved_path != NULL) {
        strcpy(resolved_path, path);
    }

    return resolved_path;
}