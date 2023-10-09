#ifndef _STDLIB_H
#define _STDLIB_H 1
#include <stddef.h>
#include <limits.h>
#include <sys/wait.h>
#include <math.h>

#define __DECLARE_SIZE_T
#define __DECLARE_WCHAR_T
#include "__posix_types.h"

/// Successful termination for exit()
#define EXIT_SUCCESS 0

/// Unsuccessful termination for exit()
#define EXIT_FAILURE 1


/// Maximum value returned by rand()
#define RAND_MAX 32767

/// Maximum number of bytes in a multibyte character
#define MB_CUR_MAX 1

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
  int quot;
  int rem;
} div_t;

typedef struct {
  long int quot;
  long int rem;
} ldiv_t;

typedef struct {
  long long quot;
  long long rem;
} lldiv_t;

/* String conversion */
double atof (const char* str);

int atoi(const char *nptr);
long int atol(const char *nptr);
long long int atoll(const char *nptr);

/**
 * @brief Convert string to double-precision floating-point number.
 *
 * The `strtod` function converts the initial portion of the string pointed to by `str`
 * to a `double` representation. It stops when the first unrecognized character is encountered.
 * If `endptr` is not a null pointer, the function stores the address of the first invalid character
 * in `*endptr`. If `str` does not point to a valid floating-point number, or if no digits were found,
 * `strtod` returns 0.0. If the converted value is outside the range of representable values for `double`,
 * the result is undefined, and `HUGE_VAL` or `HUGE_VALF` may be returned. The function recognizes
 * an optional initial whitespace, an optional sign (+ or -), an optional prefix (0x for hexadecimal
 * numbers), decimal digits, and an optional decimal point.
 *
 * @param str Pointer to the null-terminated string to be converted.
 * @param endptr Pointer to a pointer that will be updated to point to the first invalid character.
 * @return The converted double-precision floating-point number, or 0.0 if no valid digits were found.
 */
double strtod(const char *str, char **endptr);

float strtof(const char * nptr,
            char ** endptr);
long double strtold(const char * nptr,
            char ** endptr);

long int strtol(
                const char * nptr,
                char ** endptr,
                int base);
long long int strtoll(
                const char * nptr,
                char ** endptr,
                int base);
unsigned long int strtoul(
                const char * nptr,
                char ** endptr,
                int base);
unsigned long long int strtoull(
                const char * nptr,
                char ** endptr,
                int base);

/* Pseudo-random */
int rand (void);

void srand (unsigned int seed);


#ifdef __STDC_HOSTED__

/* Dynamic memory */
void* calloc (size_t num, size_t size);
void *malloc(size_t size);
void free(void*);
void* realloc (void* ptr, size_t size);

/**
 * @brief Terminate the program abnormally.
 *
 * The `abort` function terminates the program abnormally by generating a program termination
 * signal (SIGABRT). It is usually called when an unrecoverable error or critical condition is
 * encountered in the program. The function performs abnormal termination without any cleanup
 * or unwinding of the stack, so it is recommended to only use it in exceptional cases where
 * normal program flow cannot continue.
 *
 * @warning The `abort` function does not return to the caller and does not execute any
 *          registered exit handlers or cleanup functions.
 */
void abort(void);

/**
 * @brief Register a function to be called at program exit.
 *
 * The `atexit` function registers the function pointed to by `func` to be called
 * automatically when the program terminates normally using the `exit` function. Multiple
 * functions can be registered in the order in which they are called by successive calls
 * to `atexit`.
 *
 * The registered functions are called in reverse order of their registration, which means
 * that the last function registered is called first, followed by the second-to-last, and
 * so on. Each registered function is called without any arguments.
 *
 * @param func Pointer to the function to be called at program exit.
 * @return 0 on success, or a nonzero value if the maximum number of registered functions
 *         has been reached, or if an error occurred.
 */
int atexit(void (*func)(void));

void exit(int status);
void _Exit(int status);
char *getenv(const char *name);

/**
 * @brief Set the value of an environment variable or create a new one.
 *
 * The `setenv` function is used to set the value of an environment variable
 * or create a new one variable if it does not exist. If the specified
 * environment variable already exists, its value is updated with the provided one.
 *
 * @param name    A pointer to a null-terminated string containing the name of
 *                the environment variable. Must not contain the '=' character.
 * @param value   A pointer to a null-terminated string containing the new value
 *                to be associated with the environment variable. If this parameter
 *                is NULL, the environment variable will be removed from the environment.
 * @param overwrite If this parameter is non-zero and the environment variable
 *                already exists, its value will be overwritten with the provided
 *                value, and if the value is NULL, it will be removed. If `overwrite`
 *                is zero and the variable exists, the function will have no effect.
 * @return        Upon successful completion, the `setenv` function returns zero (0).
 *                If an error occurs, it returns -1, and sets the `errno` variable to
 *                indicate the error.
 *
 * @note          The `setenv` function is typically used to modify or create
 *                environment variables within a process.
 * 
 * @note          NULL may be passed as the `value` parameter to remove the
 *                variable from the environment. This is not a standard POSIX
 *                feature and should be avoided in portable applications.
 *                Instead, `unsetenv` can be used to achieve the same effect.
 *
 * @warning       Modifying or creating environment variables can affect the behavior
 *                of the entire process and should be used with caution.
 */
int setenv(const char *name, const char *value, int overwrite);

int putenv(char *string);
int system(const char *string);

void *bsearch(const void *key, const void *base,
            size_t nmemb, size_t size,
            int (*compar)(const void *, const void *));
void qsort(void *base, size_t nmemb, size_t size,
            int (*compar)(const void *, const void *));

int abs(int j);
long int labs(long int j);
long long int llabs(long long int j);
div_t div(int numer, int denom);
ldiv_t ldiv(long int numer, long int denom);
lldiv_t lldiv(long long int numer, long long int denom);

int mblen(const char *s, size_t n);
int mbtowc(wchar_t * pwc,
            const char * s,
            size_t n);
int wctomb(char *s, wchar_t wc);

size_t mbstowcs(wchar_t * pwcs,
                const char * s,
                size_t n);
size_t wcstombs(char * s,
                const wchar_t * pwcs,
                size_t n);

/**
 * @brief Create a unique temporary filename.
 *
 * This function generates a unique temporary filename based on the template
 * provided in `template`. The `template` should include at least six 'X'
 * characters at the end, which will be replaced with a combination of letters
 * and digits to ensure the filename's uniqueness.
 *
 * The function modifies the `template` string to contain the generated
 * filename. If a unique filename is created, it returns a pointer to the
 * modified `template`. If it encounters an error, it returns NULL.
 *
 * @param t A character string containing the filename template with at
 *                 least six 'X' characters at the end.
 *
 * @return A pointer to the modified `template` with a unique filename, or NULL
 * if an error occurs.
 */
char *mktemp(char * t);
// template can't be used as a variable name as it's a C++ keyword

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif