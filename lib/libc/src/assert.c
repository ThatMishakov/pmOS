#include <stdlib.h>
#include <stdio.h>

void _assert_fail(const char* condition, const char* file, unsigned int line) {
    fprintf(stderr, "Assertion failed: %s, file %s, line %u\n", condition, file, line);
    exit(EXIT_FAILURE);
}