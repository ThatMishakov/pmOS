#ifndef _STRING_H
#define _STRING_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef unsigned long size_t;

/**
 * @brief Copy memory area with limited length, stopping after a specific character is found.
 *
 * The `memccpy` function copies bytes from the source memory area `src` to the destination memory area `dest`,
 * up to a maximum of `n` bytes. The copying stops and the function returns a pointer to the byte immediately
 * following the copied character `c` if it is found in `src`, or `NULL` if `c` is not found within `n` bytes.
 *
 * @param dest Pointer to the destination memory area.
 * @param src Pointer to the source memory area.
 * @param c The character to search for.
 * @param n The maximum number of bytes to copy.
 * @return A pointer to the byte immediately following the copied character `c` if found, or `NULL` otherwise.
 */
void *memccpy(void *dest, const void *src, int c, size_t n);

/**
 * @brief Locate the first occurrence of a character in a memory block.
 *
 * The `memchr` function searches within the first `n` bytes of the memory block pointed to by `s`
 * for the first occurrence of the character `c`. It returns a pointer to the matching character
 * if found, or `NULL` if the character is not found within the specified number of bytes.
 *
 * @param s Pointer to the memory block to search in.
 * @param c The character to search for.
 * @param n The maximum number of bytes to search.
 * @return A pointer to the matching character if found, or `NULL` otherwise.
 */
void *memchr(const void *s, int c, size_t n);

/**
 * @brief Compare two memory blocks.
 *
 * The `memcmp` function compares the first `n` bytes of the memory blocks `s1` and `s2`.
 * It returns an integer value indicating the relationship between the memory blocks.
 *
 * @param s1 Pointer to the first memory block.
 * @param s2 Pointer to the second memory block.
 * @param n The number of bytes to compare.
 * @return An integer value less than, equal to, or greater than zero if the first `n` bytes
 *         of `s1` are found to be less than, equal to, or greater than the corresponding bytes
 *         of `s2`, respectively.
 */
int memcmp(const void *s1, const void *s2, size_t n);

void* memcpy(void* to, const void* from, size_t size);
void *memmove(void *to, const void *from, size_t size);
void *memset(void *str, int c, size_t n);
int strcmp (const char * str1, const char * str2);
size_t strlen (const char * str);
void *realloc(void *ptr, size_t size);

/**
 * @brief Compare two strings up to a specified length.
 *
 * The `strncmp` function compares the string `s1` to the string `s2`, up to `n` characters,
 * without regard to case. It returns an integer greater than, equal to, or less than zero,
 * depending on whether `s1` is greater than, equal to, or less than `s2`.
 *
 * @param s1 The first string to compare.
 * @param s2 The second string to compare.
 * @param n The maximum number of characters to compare.
 * @return An integer greater than, equal to, or less than zero if `s1` is greater than, equal to,
 *         or less than `s2`, respectively.
 */
int strncmp(const char *s1, const char *s2, size_t n);

/**
 * @brief Split a string into tokens.
 *
 * The `strtok` function breaks the string `str` into a series of tokens
 * using the delimiter characters specified in `delim`. On the first call,
 * `str` should point to the string to be tokenized, and subsequent calls
 * with `str` set to NULL will continue tokenizing the string.
 *
 * @param str The string to be tokenized. On subsequent calls, should be NULL.
 * @param delim The set of delimiter characters.
 * @return A pointer to the next token found in `str`, or NULL if there are no more tokens.
 */
char *strtok(char *str, const char *delim);

/**
 * @brief Split a string into tokens (thread-safe version).
 *
 * The `strtok_r` function is a reentrant version of `strtok` that uses the
 * provided save pointer `saveptr` to maintain the position of the string being
 * tokenized across multiple calls. It breaks the string `str` into a series of tokens
 * using the delimiter characters specified in `delim`. On the first call,
 * `str` should point to the string to be tokenized, and subsequent calls with
 * `str` set to NULL will continue tokenizing the string.
 *
 * @param str The string to be tokenized. On subsequent calls, should be NULL.
 * @param delim The set of delimiter characters.
 * @param saveptr A pointer to a char* variable to store the context between calls.
 * @return A pointer to the next token found in `str`, or NULL if there are no more tokens.
 */
char *strtok_r(char *str, const char *delim, char **saveptr);


#ifdef __STDC_WANT_LIB_EXT1__
typedef rsize_t size_t;
/**
 * @brief Split a string into tokens with bounds checking.
 *
 * The `strtok_s` function is a safer version of `strtok` that breaks the string `s1` into a series
 * of tokens using the delimiter characters specified in `s2`. The input string `s1` is modified
 * during tokenization, and the position of the next token is maintained using the `ptr` parameter.
 * Additionally, the `s1max` parameter represents the maximum length of the input string `s1` to
 * provide bounds checking and prevent buffer overflows.
 *
 * @param s1 The input string to be tokenized. On subsequent calls, should be NULL.
 * @param s1max A pointer to the maximum length of the input string `s1`.
 * @param s2 The set of delimiter characters.
 * @param ptr A pointer to a char pointer to store the context between calls.
 * @return A pointer to the next token found in `s1`, or NULL if there are no more tokens.
 */
char *strtok_s(char *s1, rsize_t *s1max, const char *s2, char **ptr);
#endif

/**
 * @brief Split a string into tokens.
 *
 * The `strsep` function is similar to `strtok` and breaks the string `str` into a series of tokens
 * using the delimiter characters specified in `delim`. However, unlike `strtok`, `strsep` does not
 * require the caller to provide context for subsequent calls. Each call to `strsep` returns the next
 * token found in `str`, and updates `str` to point to the remaining portion of the string.
 *
 * @param str The string to be tokenized.
 * @param delim The set of delimiter characters.
 * @return A pointer to the next token found in `str`, or NULL if there are no more tokens.
 */
char *strsep(char **str, const char *delim);

/**
 * @brief Locate the first occurrence of any character from a set in a string.
 *
 * The `strpbrk` function searches the string `str` for the first occurrence
 * of any character from the string `charset`. The search starts from the
 * beginning of `str` and stops at the first matching character found.
 *
 * @param str The string to search in.
 * @param charset The set of characters to search for.
 * @return A pointer to the first matching character in `str`,
 *         or NULL if no match is found.
 */
char *strpbrk(const char *str, const char *charset);

/**
 * @brief Duplicate a string.
 *
 * The `strdup` function creates a duplicate of the string `s` by allocating memory
 * for a new string and copying the contents of `s` into it. The new string is null-terminated.
 *
 * @param s The string to be duplicated.
 * @return A pointer to the newly allocated duplicate string, or `NULL` if the allocation fails.
 */
char *strdup(const char *s);


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif