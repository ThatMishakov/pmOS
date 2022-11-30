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
    
    while (num > 0 && *source != '\0') {
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
    size_t i = 0;
    while (s1[i] == s2[i] && s1[i]) ++i;

    if (!s1[i] && !s2[i]) return 0;
    if (s1[i] < s2[i]) return -1;
    return 1; 
}

void *memcpy(void *dest, const void * src, size_t n)
{
    void* k = dest;
    while (n--) {
        *((char*)dest++) = *((char*)src++);
    }

    return k;
}

void *memset(void *s, int c, size_t n)
{
    void *k = s;
    while (n--) {
        *((unsigned char*)(s)++) = (unsigned char)n;
    }

    return k;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    size_t k = 0;
    for (; k < n && ((unsigned char*)s1)[k] == ((unsigned char*)s2)[k]; ++k) ;

    if (k == n) return 0;

    return ((unsigned char*)s1)[k] < ((unsigned char*)s2)[k] ? -1 : 1; 
}