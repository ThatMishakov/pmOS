#ifndef _STRING_H
#define _STRING_H 1
#include <stddef.h>
#include "stdlib_com.h"
#if defined(__cplusplus)
extern "C" {
#endif


/* Copying functions */
void * memcpy ( void * destination, const void * source, size_t num );
void * memmove ( void * destination, const void * source, size_t num );
char * strcpy ( char * destination, const char * source );
char * strncpy ( char * destination, const char * source, size_t num );

/* Concatenation functions */
char * strcat ( char * destination, const char * source );
char * strncat ( char * destination, const char * source, size_t num );

/* Comparison functions */
int memcmp ( const void * ptr1, const void * ptr2, size_t num );
int strcmp ( const char * str1, const char * str2 );
int strcoll ( const char * str1, const char * str2 );
int strncmp ( const char * str1, const char * str2, size_t num );
size_t strxfrm ( char * destination, const char * source, size_t num );\

/* Search functions */
void * memchr ( const void * ptr, int value, size_t num );
char *strchr(const char *s, int c);
size_t strcspn(const char *s1, const char *s2);
char *strpbrk(const char *s1, const char *s2);
char *strrchr(const char *s, int c);
size_t strspn(const char *s1, const char *s2);
char *strstr(const char *s1, const char *s2);
char *strtok(char * s1, const char * s2);

/* Miscellaneous functions */
void *memset(void *s, int c, size_t n);
const char *strerror(int errnum);
size_t strlen(const char* str);


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif