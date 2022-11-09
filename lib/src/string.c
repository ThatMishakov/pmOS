#include "../include/string.h"

size_t strlen(char* str)
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