#ifndef _STRING_H
#define _STRING_H 1
#include <stddef.h>
#include "stdlib_com.h"
#if defined(__cplusplus)
extern "C" {
#endif


/* Copying functions */
void * memcpy ( void * destination, const void * source, size_t num );
void * memmove ( void * destination, const void * source, size_t num );
char * strcpy ( char * destination, const char * source );
char * strncpy ( char * destination, const char * source, size_t num );

/* Concatenation functions */
char * strcat ( char * destination, const char * source );
char * strncat ( char * destination, const char * source, size_t num );

/* Comparison functions */
int memcmp ( const void * ptr1, const void * ptr2, size_t num );
int strcmp ( const char * str1, const char * str2 );

/**
 * @brief Compare two strings using the current locale's collation rules.
 *
 * The `strcoll` function compares the two strings `s1` and `s2` using the current locale's
 * collation rules. It performs a lexicographic comparison and returns an integer less than,
 * equal to, or greater than zero if `s1` is found to be less than, equal to, or greater than `s2`,
 * respectively.
 *
 * @param s1 The first null-terminated string to compare.
 * @param s2 The second null-terminated string to compare.
 * @return An integer less than, equal to, or greater than zero if `s1` is found to be less than,
 *         equal to, or greater than `s2`, respectively.
 */
int strcoll(const char *s1, const char *s2);

int strncmp ( const char * str1, const char * str2, size_t num );
size_t strxfrm ( char * destination, const char * source, size_t num );

/* Search functions */
void * memchr ( const void * ptr, int value, size_t num );
char *strchr(const char *s, int c);
size_t strcspn(const char *s1, const char *s2);
char *strpbrk(const char *s1, const char *s2);
char *strrchr(const char *s, int c);
size_t strspn(const char *s1, const char *s2);
char *strstr(const char *s1, const char *s2);
char *strtok(char * s1, const char * s2);

/* Miscellaneous functions */
void *memset(void *s, int c, size_t n);
const char *strerror(int errnum);
size_t strlen(const char* str);

/**
 * @brief Get the length of a string, limited by a maximum number of characters.
 *
 * The strnlen function calculates the length of the null-terminated string pointed
 * to by `s`, but limits the length to a maximum of `maxlen` characters.
 *
 * @param s       Pointer to the null-terminated string.
 * @param maxlen  Maximum number of characters to consider.
 *
 * @return The length of the string, limited by `maxlen`, without including the null terminator.
 *
 * @note If no null terminator is found within the specified length `maxlen`, the function returns `maxlen`.
 * @note The behavior is undefined if `s` is not a valid pointer to a null-terminated string.
 *
 * @see strlen
 */
size_t strnlen(const char *s, size_t maxlen);

/**
 * @brief Duplicate a string.
 *
 * The `strdup` function allocates memory for a new string containing a duplicate of the null-terminated string `s`.
 *
 * @param s The null-terminated string to duplicate.
 * @return A pointer to the new string, or `NULL` if the allocation fails.
 */
char *strdup(const char *s);

#if _POSIX_C_SOURCE >= 200809L
/**
 * @brief Duplicate a portion of a string with limited length.
 *
 * The `strndup` function duplicates at most `n` bytes from the string `str`, creating
 * a new null-terminated string. The result is stored in dynamically allocated memory,
 * which should be released using the `free` function when it is no longer needed.
 *
 * If the length of the original string is less than `n`, the entire string is duplicated.
 *
 * @param str Pointer to the null-terminated string to be duplicated.
 * @param n   The maximum number of bytes to duplicate.
 * @return A pointer to the newly allocated string on success, or NULL on failure.
 */
char *strndup(const char *str, size_t n);
#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif