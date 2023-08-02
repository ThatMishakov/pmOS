#ifndef _STDIO_H
#define _STDIO_H 1
#include "stdlib_com.h"
#include <stdarg.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef __STDC_HOSTED__

typedef uint64_t pmos_port_t;

typedef struct FILE {
    union {
        struct {
            pmos_port_t port_ptr;
            const char* port_name;
            size_t size;
            uint32_t flags;
        } port;

        struct {
            uint8_t* buffer;
            size_t size;
            size_t pos;
            uint32_t flags;
        } string;
    };
    uint8_t type;
} 
FILE;
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

/**
 * @brief Set the file position indicator for a stream.
 *
 * The `fseek` function sets the file position indicator for the given stream `stream`
 * to the specified offset `offset` from the reference point `whence`. The reference point
 * can be one of the following constants:
 *
 * - `SEEK_SET`: The offset is relative to the beginning of the file.
 * - `SEEK_CUR`: The offset is relative to the current file position.
 * - `SEEK_END`: The offset is relative to the end of the file.
 *
 * After a successful call to `fseek`, the next operation on the stream, such as a read or write,
 * will occur at the specified position. If `fseek` encounters an error, it will return a nonzero
 * value, and the file position indicator may be unchanged.
 *
 * @param stream Pointer to the FILE object representing the stream.
 * @param offset The offset value to set the file position indicator to.
 * @param whence The reference point from which to calculate the offset.
 * @return 0 on success, or a nonzero value if an error occurred.
 */
int fseek(FILE *stream, long int offset, int whence);

int fsetpos ( FILE * stream, const fpos_t * pos );
long int ftell ( FILE * stream );
void rewind ( FILE * stream );

void clearerr ( FILE * stream );
int feof ( FILE * stream );
int ferror ( FILE * stream );
void perror ( const char * str );

FILE    *fdopen(int, const char *);
int      fileno(FILE *);

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

#define _IONBF 0
#define _IOLBF 1
#define _IOFBF 2

#define FP_NAN 0
#define FP_INFINITE 1
#define FP_NORMAL 2
#define FP_SUBNORMAL 3
#define FP_ZERO 4

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif


#endif // _STDIO_H