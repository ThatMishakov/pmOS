#ifndef _STDIO_H
#define _STDIO_H 1

#define __DECLARE_SIZE_T
#define __DECLARE_SSIZE_T
#define __DECLARE_OFF_T
#include "__posix_types.h"

// This shouldn't be included here, but this header is broken without it.
#include <stdint.h>

#include <stddef.h>
#include <stdarg.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef unsigned long pmos_port_t;

// FILE * is actually POSIX file descriptor (int)
typedef void FILE;

typedef size_t fpos_t;

typedef int errno_t;
typedef size_t rsize_t;

#ifdef __STDC_HOSTED__

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

/**
 * @brief Format a string with a maximum length, using a va_list.
 *
 * This function writes formatted data to a character string `str` with a
 * specified maximum `size`, using the format and arguments provided in
 * `format` and `ap` (a `va_list`).
 *
 * @param str   A pointer to the destination character string.
 * @param size  The maximum number of characters to write, including the null
 *              terminator. If the formatted output exceeds this size, it will
 *              be truncated.
 * @param format A format string that specifies how the following arguments are
 *               formatted.
 * @param ap    A `va_list` containing the arguments to be formatted and
 *              inserted into the resulting string.
 *
 * @return The number of characters written (excluding the null terminator) if
 * successful. If the output is truncated, it returns the number of characters
 * that would have been written if the size were sufficiently large (not
 * counting the null terminator). If an error occurs, it returns a negative
 * value.
 */
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

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

static FILE * stdin = (FILE *) 0;
static FILE * stdout = (FILE *) 1;
static FILE * stderr = (FILE *) 2;

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