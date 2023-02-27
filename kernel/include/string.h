#ifndef _STRING_H
#define _STRING_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef unsigned long size_t;

void memcpy(void* to, const void* from, size_t size);
void *memset(void *str, int c, size_t n);
int strcmp (const char * str1, const char * str2);
size_t strlen (const char * str);
void *realloc(void *ptr, size_t size);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif