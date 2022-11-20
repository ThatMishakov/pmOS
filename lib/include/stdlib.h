#ifndef _STDLIB_H
#define _STDLIB_H 1
#include <stddef.h>

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

typedef unsigned long size_t;

/* String conversion */
double atof (const char* str);

int atoi(const char *nptr);
long int atol(const char *nptr);
long long int atoll(const char *nptr);

double strtod(const char * nptr,
            char ** endptr);
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


/* Dynamic memory */
void* calloc (size_t num, size_t size);
void *malloc(size_t size);
void free(void*);
void* realloc (void* ptr, size_t size);

void abort(void);
int atexit(void (*func)(void));
void exit(int status);
void _Exit(int status);
char *getenv(const char *name);
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

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define NULL 0L

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif