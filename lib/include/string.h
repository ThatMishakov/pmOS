#ifndef _STRING_H
#define _STRING_H 1
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

size_t strlen(char* str);
void * memcpy ( void * destination, const void * source, size_t num );
void * memmove ( void * destination, const void * source, size_t num );
char * strcpy ( char * destination, const char * source );
char * strncpy ( char * destination, const char * source, size_t num );

char * strcat ( char * destination, const char * source );
char * strncat ( char * destination, const char * source, size_t num );

int memcmp ( const void * ptr1, const void * ptr2, size_t num );
int strcmp ( const char * str1, const char * str2 );
int strcoll ( const char * str1, const char * str2 );
int strncmp ( const char * str1, const char * str2, size_t num );

size_t strxfrm ( char * destination, const char * source, size_t num );
const void * memchr ( const void * ptr, int value, size_t num );




#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif