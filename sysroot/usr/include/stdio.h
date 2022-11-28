#ifndef _STDIO_H
#define _STDIO_H 1
#include "stdlib_com.h"
#include <stdarg.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct FILE {
    uint64_t port_ptr;
    size_t size;
    uint32_t flags;
    uint8_t type;
} FILE;
typedef size_t fpos_t;

typedef int errno_t;
typedef size_t rsize_t;


int remove ( const char * filename );
int rename ( const char * oldname, const char * newname );
FILE * tmpfile ( void );
char * tmpnam ( char * str );

int fclose ( FILE * stream );
int fflush ( FILE * stream );
FILE * fopen ( const char * filename, const char * mode );
FILE * freopen ( const char * filename, const char * mode, FILE * stream );
void setbuf ( FILE * stream, char * buffer );
int setvbuf ( FILE * stream, char * buffer, int mode, size_t size );

int fprintf ( FILE * stream, const char * format, ... );
int fscanf ( FILE * stream, const char * format, ... );
int printf ( const char * format, ... );
int scanf ( const char * format, ... );
int snprintf ( char * s, size_t n, const char * format, ... );
int sprintf ( char * str, const char * format, ... );
int sscanf ( const char * s, const char * format, ...);
int vfprintf ( FILE * stream, const char * format, va_list arg );
int vfscanf ( FILE * stream, const char * format, va_list arg );
int vprintf ( const char * format, va_list arg );
int vscanf ( const char * format, va_list arg );
int vsnprintf (char * s, size_t n, const char * format, va_list arg );
int vsprintf (char * s, const char * format, va_list arg );
int vsscanf ( const char * s, const char * format, va_list arg );

int fgetc ( FILE * stream );
char * fgets ( char * str, int num, FILE * stream );
int fputc ( int character, FILE * stream );
int fputs ( const char * str, FILE * stream );
int getc ( FILE * stream );
int getchar ( void );
char * gets ( char * str );
int putc ( int character, FILE * stream );
int putchar ( int character );
int puts ( const char * str );
int ungetc ( int character, FILE * stream );

size_t fread ( void * ptr, size_t size, size_t count, FILE * stream );
size_t fwrite ( const void * ptr, size_t size, size_t count, FILE * stream );

int fgetpos ( FILE * stream, fpos_t * pos );
int fseek ( FILE * stream, long int offset, int origin );
int fsetpos ( FILE * stream, const fpos_t * pos );
long int ftell ( FILE * stream );
void rewind ( FILE * stream );

void clearerr ( FILE * stream );
int feof ( FILE * stream );
int ferror ( FILE * stream );
void perror ( const char * str );

#define BUFSIZ 4096
#define EOF -1
#define FILENAME_MAX 65536
#define FOPEN_MAX 65536
#define L_tmpnam 1
#define TMP_MAX 65536

extern FILE * stdin;
extern FILE * stdout;
extern FILE * stderr;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#if defined(__cplusplus)
} /* extern "C" */
#endif


#endif // _STDIO_H