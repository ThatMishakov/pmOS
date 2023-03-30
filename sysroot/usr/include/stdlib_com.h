#ifndef _STDLIB_COM_H
#define _STDLIB_COM_H 1

#if defined(__cplusplus)
extern "C" {
#endif

typedef unsigned long size_t;

// This is probably wrong, but gcc/libquadmath/printf/quadmath-printf.c requires ssize_t and I couldn't find
// headers other than sys/types.h where it should be declared
typedef signed long ssize_t; 
#define NULL 0L

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif // _STDLIB_COM_H