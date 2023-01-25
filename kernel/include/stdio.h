#ifndef _STDIO_H
#define _STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FILE {
    unsigned long channel;
    unsigned long type;
} FILE;

int fprintf(FILE *stream, const char *format, ...);
int printf(const char *format, ...);

extern FILE * stderr;

#ifdef __cplusplus
}
#endif

#endif