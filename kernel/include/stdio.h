#ifndef _STDIO_H
#define _STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long FILE;

static FILE zero = 0;
static FILE *stderr = &zero;


int fprintf(FILE *stream, const char *format, ...);
int printf(const char *format, ...);
int fflush(FILE *stream);

#ifdef __cplusplus
}
#endif

#endif