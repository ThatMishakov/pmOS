/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _STRING_H
#define _STRING_H 1

#define __DECLARE_SIZE_T
#define __DECLARE_NULL
#define __DECLARE_LOCALE_T
#include "__posix_types.h"


#if defined(__cplusplus)
extern "C" {
#endif


/* Copying functions */
void * memcpy ( void * destination, const void * source, size_t num );
void * memmove ( void * destination, const void * source, size_t num );

/**
 * @brief Copy the string from source to destination, including the NULL terminator.
 * 
 * @param destination Destination for the copy.
 * @param source Source string to copy. NULL-termination denotes its end.
 * @return A pointer to the destination string.
*/
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

/**
 * @brief Find the first occurrence of a character in a string.
 * 
 * This function returns the length of the initial part of s1 that does not contain any characters that are
 * part of s2. If no characters from s2 are found, the length of s1 is returned.
 * 
 * @param s1 The string to be searched.
 * @param s2 The characters to search for.
 * @return The length of the initial part of s1 that does not contain any characters that are part of s2.
*/
size_t strcspn(const char *s1, const char *s2);
char *strpbrk(const char *s1, const char *s2);
char *strrchr(const char *s, int c);
size_t strspn(const char *s1, const char *s2);
char *strstr(const char *s1, const char *s2);
char *strtok(char * s1, const char * s2);

/* Miscellaneous functions */
void *memset(void *s, int c, size_t n);

/** 
 * @brief Get an error message string for an error number.
 * 
 * The `strerror` function returns a pointer to a string that describes the error code
 * passed in the argument `errnum`, possibly using the LC_MESSAGES part of the current locale
 * to select the appropriate language. The string is not guaranteed to be unique for each
 * distinct `errnum` value.
 * 
 * @param errnum The error number to get a message string for.
 * @return A pointer to a string describing the error code `errnum`.
 */
char *strerror(int errnum);
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

/**
 * @brief Copy the string from str to dst, up to the maximum length sz
 *        and return a pointer to the end of the copied string.
 * 
 * The stpncpy function is similar to the strncpy function, except that
 * it returns a pointer to the end of the copied string.
 * 
 * @param dst Destination.
 * @param src Source string. NULL-termination denotes its end.
 * @return A pointer to one after the last character in the destination sequence.
*/
char *stpncpy(char *dst, const char * src, size_t sz);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif