#include "../include/string.h"

size_t strlen(const char* str)
{
    size_t size = 0;
    for(;str[size] != '\0'; ++size);

    return size;
}

char * strncpy ( char * destination, const char * source, size_t num )
{
    char* p = destination;
    
    while (num > 0 && *p != '\0') {
        *p = *source;
        ++p; ++source;
        --num;
    }

    while (num-- > 0) {
        *p = '\0';
        ++p;
    }

    return destination;
}

int strcmp(const char *s1, const char *s2)
{
    int i = 0;
    while (s1[i] == s2[i] && s1[i]) ++i;

    if (!s1[i] && !s2[i]) return 0;
    if (s1[i] < s2[i]) return -1;
    return 1; 
}