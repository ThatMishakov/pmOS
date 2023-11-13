#ifndef _STRING_H
#define _STRING_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef unsigned long size_t;

// Locale is not yet supported
typedef unsigned long locale_t;

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

/**
 * @brief Copy a block of memory.
 *
 * The `memcpy` function copies `n` bytes from the memory area pointed to by `src`
 * to the memory area pointed to by `dest`. The memory areas must not overlap.
 *
 * @param dest Pointer to the destination memory area.
 * @param src Pointer to the source memory area.
 * @param n The number of bytes to copy.
 * @return A pointer to the destination memory area (`dest`).
 */
void *memcpy(void *dest, const void *src, size_t n);


/**
 * @brief Copy a block of memory, handling possible overlap.
 *
 * The `memmove` function copies `n` bytes from the memory area pointed to by `src`
 * to the memory area pointed to by `dest`. The memory areas can overlap.
 *
 * @param dest Pointer to the destination memory area.
 * @param src Pointer to the source memory area.
 * @param n The number of bytes to copy.
 * @return A pointer to the destination memory area (`dest`).
 */
void *memmove(void *dest, const void *src, size_t n);


/**
 * @brief Fill a block of memory with a specified value.
 *
 * The `memset` function fills the first `n` bytes of the memory area pointed to by `s`
 * with the constant byte `c`.
 *
 * @param s Pointer to the memory area to be filled.
 * @param c The value to be set (`unsigned char` type).
 * @param n The number of bytes to fill.
 * @return A pointer to the memory area (`s`).
 */
void *memset(void *s, int c, size_t n);

/**
 * @brief Copy a string and return a pointer to the terminating null character.
 *
 * The `stpcpy` function copies the string `src` to the memory location `dest` and
 * returns a pointer to the terminating null character of the copied string.
 *
 * @param dest Pointer to the destination string.
 * @param src Pointer to the source string.
 * @return A pointer to the terminating null character of the copied string (`dest` + length of `src`).
 */
char *stpcpy(char *dest, const char *src);


/**
 * @brief Copy a limited length of a string and return a pointer to the terminating null character.
 *
 * The `stpncpy` function copies at most `n` characters from the string `src` to the memory
 * location `dest` and returns a pointer to the terminating null character of the copied string.
 * If the length of `src` is less than `n`, the remaining characters in `dest` are filled with null bytes.
 *
 * @param dest Pointer to the destination string.
 * @param src Pointer to the source string.
 * @param n The maximum number of characters to copy.
 * @return A pointer to the terminating null character of the copied string (`dest` + length of `src`
 *         or `n`, whichever is smaller).
 */
char *stpncpy(char *dest, const char *src, size_t n);


/**
 * @brief Concatenate two strings.
 *
 * The `strcat` function appends a copy of the string `src` to the end of the string `dest`.
 * The initial character of `src` overwrites the null character at the end of `dest`,
 * and a null character is appended to the resulting string.
 *
 * @param dest Pointer to the destination string.
 * @param src Pointer to the source string.
 * @return A pointer to the resulting string (`dest`).
 */
char *strcat(char *dest, const char *src);

/**
 * @brief Locate the first occurrence of a character in a string.
 *
 * The `strchr` function searches for the first occurrence of the character `c`
 * in the string `s` and returns a pointer to that character or a null pointer if `c`
 * is not found in the string.
 *
 * @param s Pointer to the string to search in.
 * @param c The character to search for.
 * @return A pointer to the first occurrence of `c` in `s`, or `NULL` if `c` is not found.
 */
char *strchr(const char *s, int c);


/**
 * @brief Compare two strings.
 *
 * The `strcmp` function compares the string `s1` to the string `s2` and returns an integer
 * less than, equal to, or greater than zero if `s1` is found to be less than, equal to,
 * or greater than `s2`, respectively.
 *
 * @param s1 Pointer to the first string.
 * @param s2 Pointer to the second string.
 * @return An integer less than, equal to, or greater than zero if `s1` is found to be
 *         less than, equal to, or greater than `s2`, respectively.
 */
int strcmp(const char *s1, const char *s2);


/**
 * @brief Compare two strings using the locale-specific collation order.
 *
 * The `strcoll` function compares the string `s1` to the string `s2` using the rules
 * defined by the current locale's collation order. It returns an integer less than,
 * equal to, or greater than zero if `s1` is found to be less than, equal to, or greater
 * than `s2`, respectively, in the collation order.
 *
 * @param s1 Pointer to the first string.
 * @param s2 Pointer to the second string.
 * @return An integer less than, equal to, or greater than zero if `s1` is found to be
 *         less than, equal to, or greater than `s2`, respectively, in the collation order.
 */
int strcoll(const char *s1, const char *s2);

/**
 * @brief Compare two strings using a specific locale.
 *
 * The `strcoll_l` function compares the string `s1` to the string `s2` using the rules
 * defined by the specified locale `locale`. It returns an integer less than, equal to,
 * or greater than zero if `s1` is found to be less than, equal to, or greater than `s2`,
 * respectively, in the collation order of the specified locale.
 *
 * @param s1 Pointer to the first string.
 * @param s2 Pointer to the second string.
 * @param locale Locale to be used for collation.
 * @return An integer less than, equal to, or greater than zero if `s1` is found to be
 *         less than, equal to, or greater than `s2`, respectively, in the collation order
 *         of the specified locale.
 */
int strcoll_l(const char *s1, const char *s2, locale_t locale);


/**
 * @brief Copy a string.
 *
 * The `strcpy` function copies the string `src` (including the terminating null character)
 * to the location specified by `dest`. The destination string must have enough space to
 * hold the entire contents of the source string.
 *
 * @param dest Pointer to the destination string.
 * @param src Pointer to the source string.
 * @return A pointer to the destination string (`dest`).
 */
char *strcpy(char *dest, const char *src);


/**
 * @brief Calculate the length of the initial segment of a string that does not contain any characters from a given set.
 *
 * The `strcspn` function calculates the length of the initial segment of the string `str`
 * that consists of characters not found in the string `reject`. It returns the number of
 * characters in `str` before any character from `reject` is found, or the length of `str`
 * if no such character is found.
 *
 * @param str Pointer to the string to be searched.
 * @param reject Pointer to the string containing the characters to reject.
 * @return The length of the initial segment of `str` without any characters from `reject`.
 */
size_t strcspn(const char *str, const char *reject);


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

/**
 * @brief Get the error message string corresponding to an error number.
 *
 * The `strerror` function maps the error number specified by `errnum` to an
 * implementation-defined error message string and returns a pointer to that string.
 *
 * @param errnum The error number.
 * @return A pointer to the error message string.
 */
char *strerror(int errnum);


/**
 * @brief Get the locale-specific error message string corresponding to an error number.
 *
 * The `strerror_l` function maps the error number specified by `errnum` to a
 * locale-specific error message string using the specified locale `locale`.
 * It returns a pointer to that string.
 *
 * @param errnum The error number.
 * @param locale The locale to be used for error message retrieval.
 * @return A pointer to the locale-specific error message string.
 */
char *strerror_l(int errnum, locale_t locale);


/**
 * @brief Get the error message string corresponding to an error number, thread-safe version.
 *
 * The `strerror_r` function maps the error number specified by `errnum` to an
 * implementation-defined error message string and stores it in the buffer `buf`
 * of size `buflen`. The function returns a pointer to `buf`.
 *
 * @param errnum The error number.
 * @param buf Pointer to the buffer to store the error message.
 * @param buflen The size of the buffer.
 * @return A pointer to the buffer `buf`.
 */
char *strerror_r(int errnum, char *buf, size_t buflen);


/**
 * @brief Get the length of a string.
 *
 * The `strlen` function calculates the length of the null-terminated string `s`,
 * excluding the terminating null character, and returns the number of characters.
 *
 * @param s Pointer to the null-terminated string.
 * @return The length of the string.
 */
size_t strlen(const char *s);


/**
 * @brief Concatenate a limited number of characters from a string to another string.
 *
 * The `strncat` function appends at most `n` characters from the null-terminated string `src`
 * to the end of the null-terminated string `dest`. The resulting string is null-terminated.
 * The destination string must have enough space to hold the combined strings.
 *
 * @param dest Pointer to the destination string.
 * @param src Pointer to the source string.
 * @param n The maximum number of characters to append.
 * @return A pointer to the resulting string (`dest`).
 */
char *strncat(char *dest, const char *src, size_t n);

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
 * @brief Copy a limited number of characters from a string to another string.
 *
 * The `strncpy` function copies at most `n` characters from the null-terminated string `src`
 * to the character array `dest`. If `src` is shorter than `n` characters, null characters are
 * appended to `dest` until a total of `n` characters have been written. If `src` is longer
 * than `n` characters, `dest` will not be null-terminated. The function returns a pointer to
 * the destination string.
 *
 * @param dest Pointer to the destination character array.
 * @param src Pointer to the source string.
 * @param n The maximum number of characters to copy.
 * @return A pointer to the destination string (`dest`).
 */
char *strncpy(char *dest, const char *src, size_t n);


/**
 * @brief Duplicate a string up to a specified length.
 *
 * The `strndup` function duplicates the null-terminated string `s` up to a maximum
 * length of `n` characters. It allocates memory for the duplicated string using `malloc`
 * and returns a pointer to the new string. The returned string is always null-terminated
 * even if `n` is reached before the end of the original string. If `s` is longer than `n`,
 * the duplicated string will have a length of `n`. If memory allocation fails, `NULL` is returned.
 *
 * @param s Pointer to the null-terminated string.
 * @param n Maximum number of characters to duplicate.
 * @return A pointer to the duplicated string, or `NULL` if memory allocation fails.
 */
char *strndup(const char *s, size_t n);


/**
 * @brief Get the length of a string up to a specified maximum length.
 *
 * The `strnlen` function calculates the length of the null-terminated string `s`,
 * up to a maximum length of `maxlen` characters. It returns the number of characters
 * in `s` up to the first null character encountered or the maximum length specified.
 * Unlike `strlen`, `strnlen` ensures that it does not read beyond the maximum length.
 *
 * @param s Pointer to the null-terminated string.
 * @param maxlen Maximum length to search.
 * @return The length of the string up to the first null character or the maximum length specified.
 */
size_t strnlen(const char *s, size_t maxlen);


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
 * @brief Locate the last occurrence of a character in a string.
 *
 * The `strrchr` function searches for the last occurrence of the character `c` in the string `s`
 * and returns a pointer to that character or a null pointer if `c` is not found in the string.
 *
 * @param s Pointer to the string to search in.
 * @param c The character to search for.
 * @return A pointer to the last occurrence of `c` in `s`, or `NULL` if `c` is not found.
 */
char *strrchr(const char *s, int c);


/**
 * @brief Get the string representation of a signal number.
 *
 * The `strsignal` function maps the signal number specified by `signum` to an
 * implementation-defined string representation of the signal's name and returns
 * a pointer to that string.
 *
 * @param signum The signal number.
 * @return A pointer to the string representation of the signal's name.
 */
char *strsignal(int signum);


/**
 * @brief Get the length of the initial segment of a string that consists of only certain characters.
 *
 * The `strspn` function calculates the length of the initial segment of the string `str`
 * that consists entirely of characters found in the string `accept`. It returns the number
 * of characters in `str` before the first occurrence of any character not found in `accept`.
 *
 * @param str Pointer to the string to be searched.
 * @param accept Pointer to the string containing the characters to accept.
 * @return The length of the initial segment of `str` consisting of only characters from `accept`.
 */
size_t strspn(const char *str, const char *accept);


/**
 * @brief Find the first occurrence of a substring within a string.
 *
 * The `strstr` function finds the first occurrence of the substring `s2` within the string `s1`.
 * It returns a pointer to the beginning of the located substring or a null pointer if the substring is not found.
 *
 * @param s1 Pointer to the string to search in.
 * @param s2 Pointer to the substring to search for.
 * @return A pointer to the beginning of the located substring, or `NULL` if the substring is not found.
 */
char *strstr(const char *s1, const char *s2);


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
 * @brief Transform a string according to the current locale.
 *
 * The `strxfrm` function transforms the string `dest` based on the string `src`,
 * according to the rules of the current locale. It performs locale-dependent
 * transformations such as collation and returns the total number of characters
 * that would have been written to `dest` if there were no size restrictions.
 * The transformed string in `dest` is guaranteed to be null-terminated.
 *
 * @param dest Pointer to the destination character array.
 * @param src Pointer to the source string.
 * @param n Maximum number of characters to be written to `dest`.
 * @return The length of the transformed string, excluding the null terminator,
 *         that would have been written to `dest` if there were no size restrictions.
 */
size_t strxfrm(char *dest, const char *src, size_t n);


/**
 * @brief Transform a string according to a specified locale.
 *
 * The `strxfrm_l` function transforms the string `dest` based on the string `src`,
 * according to the rules of the specified locale `locale`. It performs locale-dependent
 * transformations such as collation using the specified locale and returns the total
 * number of characters that would have been written to `dest` if there were no size restrictions.
 * The transformed string in `dest` is guaranteed to be null-terminated.
 *
 * @param dest Pointer to the destination character array.
 * @param src Pointer to the source string.
 * @param n Maximum number of characters to be written to `dest`.
 * @param locale The locale to be used for the transformation.
 * @return The length of the transformed string, excluding the null terminator,
 *         that would have been written to `dest` if there were no size restrictions.
 */
size_t strxfrm_l(char *dest, const char *src, size_t n, locale_t locale);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif