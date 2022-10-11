#include <utils.h>

char strcmp(char* str1, char* str2)
{
    while (*str1 != '\0') {
        if (*str1 != *str2) return 0;
        ++str1; ++str2;
    }

    if (str2 != 0) return 0;

    return 1;
}

char str_starts_with(char* str1, char* str2)
{
    while (*str2 != '\0') {
        if (*str1 == '\0' || *str1 != *str2) return 0;
        ++str1; ++str2;
    }
    return 1;
}