#ifndef _STRINGS_H
#define _STRINGS_H 1

#define __DECLARE_LOCALE_T
#define __DECLARE_SIZE_T
#include "__posix_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

int    bcmp(const void *, const void *, size_t);
void   bcopy(const void *, void *, size_t);
void   bzero(void *, size_t);
int    ffs(int);
char   *index(const char *, int);
char   *rindex(const char *, int);
int    strcasecmp(const char *, const char *);
int    strncasecmp(const char *, const char *, size_t);


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif