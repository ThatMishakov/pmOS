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

#include "../include/string.h"

#include <errno.h>
#include <stdlib.h>

size_t strlen(const char *str)
{
    size_t size = 0;
    for (; str[size] != '\0'; ++size)
        ;

    return size;
}

char *strcpy(char * restrict destination, const char * restrict source)
{
    char *p = destination;
    while (*source != '\0') {
        *p = *source;
        ++p;
        ++source;
    }

    *p = '\0';
    return destination;
}

char *strncpy(char * restrict destination, const char *restrict source, size_t num)
{
    char *p = destination;

    while (num > 0 && *source != '\0') {
        *p = *source;
        ++p;
        ++source;
        --num;
    }

    while (num-- > 0) {
        *p = '\0';
        ++p;
    }

    return destination;
}

char *stpncpy(char * restrict dst, const char * restrict src, size_t sz)
{
    char *d = dst;
    while (sz > 0 && *src != '\0') {
        *d = *src;
        ++d;
        ++src;
        --sz;
    }

    while (sz-- > 0) {
        *d = '\0';
        ++d;
    }

    return dst;
}

int strcmp(const char *s1, const char *s2)
{
    size_t i = 0;
    while (s1[i] == s2[i] && s1[i])
        ++i;

    if (!s1[i] && !s2[i])
        return 0;
    if (s1[i] < s2[i])
        return -1;
    return 1;
}

int strncmp(const char *s1, const char *s2, size_t size)
{
    size_t i = 0;
    while (i < size && s1[i] == s2[i] && s1[i])
        ++i;

    if (i == size || (!s1[i] && !s2[i]))
        return 0;
    if (s1[i] < s2[i])
        return -1;
    return 1;
}

void *memcpy(void * restrict dest, const void * restrict src, size_t n)
{
    void *k = dest;
    while (n--) {
        *((char *)dest++) = *((char *)src++);
    }

    return k;
}

void *memset(void *s, int c, size_t n)
{
    void *k = s;
    while (n--) {
        *((unsigned char *)(s)++) = (unsigned char)c;
    }

    return k;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    size_t k = 0;
    for (; k < n && ((unsigned char *)s1)[k] == ((unsigned char *)s2)[k]; ++k)
        ;

    if (k == n)
        return 0;

    return ((unsigned char *)s1)[k] < ((unsigned char *)s2)[k] ? -1 : 1;
}

void *memmove(void *restrict dest, const void *restrict src, size_t n)
{
    unsigned char *d       = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    // Check for overlap
    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

char *strchr(const char *str, int c)
{
    char ch = (char)c;
    while (*str != '\0') {
        if (*str == ch) {
            return (char *)str;
        }
        str++;
    }

    // Check if the terminating null character matches
    if (ch == '\0') {
        return (char *)str;
    }

    return NULL;
}

char *strrchr(const char *str, int c)
{
    char ch                     = (char)c;
    const char *last_occurrence = NULL;

    while (*str != '\0') {
        if (*str == ch) {
            last_occurrence = str;
        }
        str++;
    }

    // Check if the terminating null character matches
    if (ch == '\0') {
        return (char *)str;
    }

    return (char *)last_occurrence;
}

char *strtok_r(char *str, const char *delim, char **saveptr)
{
    if (str == NULL && *saveptr == NULL) {
        return NULL; // No more tokens, and no saved context
    }

    if (str != NULL) {
        *saveptr = str; // Set the initial context
    }

    // Find the start of the next token
    char *token_start = *saveptr;
    char *delim_pos   = strpbrk(token_start, delim);

    if (delim_pos != NULL) {
        *delim_pos = '\0';          // Replace delimiter with null character
        *saveptr   = delim_pos + 1; // Update the context for the next call
    } else {
        *saveptr = NULL; // No more tokens, clear the context
    }

    return token_start;
}

char *strtok(char *str, const char *delim)
{
    static char *saveptr = NULL; // Static variable to store the context

    if (str != NULL) {
        saveptr = str; // Set the initial context
    } else if (saveptr == NULL) {
        return NULL; // No more tokens, and no saved context
    }

    // Skip leading delimiters
    char *token_start = saveptr + strspn(saveptr, delim);
    if (*token_start == '\0') {
        saveptr = NULL; // No more tokens, clear the context
        return NULL;
    }

    // Find the end of the token
    char *delim_pos = strpbrk(token_start, delim);
    if (delim_pos != NULL) {
        *delim_pos = '\0';          // Replace delimiter with null character
        saveptr    = delim_pos + 1; // Update the context for the next call
    } else {
        saveptr = NULL; // No more tokens, clear the context
    }

    return token_start;
}

size_t strspn(const char *str1, const char *str2)
{
    size_t i = 0;
    while (str1[i] != '\0') {
        const char *s = str2;
        while (*s != '\0') {
            if (str1[i] == *s)
                return i;
            ++s;
        }
        ++i;
    }

    return i;
}

size_t strcspn(const char *s1, const char *s2)
{
    size_t i = 0;
    while (s1[i] != '\0') {
        const char *s = s2;
        while (*s != '\0') {
            if (s1[i] == *s)
                return i;
            ++s;
        }
        ++i;
    }

    return i;
}

char *strpbrk(const char *str, const char *charset)
{
    const char *s1 = str;
    const char *s2;

    while (*s1 != '\0') {
        s2 = charset;
        while (*s2 != '\0') {
            if (*s1 == *s2) {
                return (char *)s1; // Found a match, return the pointer to the matching character
            }
            s2++;
        }
        s1++;
    }

    return NULL; // No match found, return NULL
}

char *strdup(const char *str)
{
    size_t length   = strlen(str) + 1;
    char *duplicate = malloc(length);

    if (duplicate != NULL) {
        memcpy(duplicate, str, length);
    }

    return duplicate;
}

char *strndup(const char *str, size_t s)
{
    size_t length = strlen(str);
    if (length > s)
        length = s;

    char *duplicate = malloc(length + 1);
    if (duplicate != NULL) {
        memcpy(duplicate, str, length);
        duplicate[length] = '\0';
    }

    return duplicate;
}

size_t strnlen(const char *str, size_t maxlen)
{
    size_t length = 0;

    while (length < maxlen && str[length] != '\0') {
        length++;
    }

    return length;
}

void *memccpy(void *restrict dest, const void *restrict src, int c, size_t n)
{
    unsigned char *d       = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    while (n > 0) {
        *d = *s;
        if (*s == (unsigned char)c) {
            return d + 1; // Found the character, return pointer to next byte
        }
        d++;
        s++;
        n--;
    }

    return NULL; // Character not found within n bytes, return NULL
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s;

    for (size_t i = 0; i < n; ++i) {
        if (*p == (unsigned char)c) {
            return (void *)p; // Found the character, return pointer to it
        }
        ++p;
    }

    return NULL; // Character not found within n bytes, return NULL
}

char *strstr(const char *s, const char *cc)
{
    const char *c = cc;
    if (*c == '\0') {
        return (char *)s;
    }

    while (*s != '\0') {
        if (*s == *c) {
            const char *s1 = s;
            const char *c1 = c;
            while (*s1 != '\0' && *c1 != '\0' && *s1 == *c1) {
                ++s1;
                ++c1;
            }

            if (*c1 == '\0') {
                return (char *)s;
            }
        }
        ++s;
    }

    return NULL;
}

const char *error_msgs[] = {
    [0] = "No error",
    [E2BIG] = "Argument list too long",
    [EACCES] = "Permission denied",
    [EADDRINUSE] = "Address already in use",
    [EADDRNOTAVAIL] = "Cannot assign requested address",
    [EAFNOSUPPORT] = "Address family not supported",
    [EAGAIN] = "Resource temporarily unavailable",
    [EALREADY] = "Operation already in progress",
    [EBADF] = "Bad file descriptor",
    [EBADMSG] = "Bad message",
    [EBUSY] = "Device or resource busy",
    [ECANCELED] = "Operation canceled",
    [ECHILD] = "No child processes",
    [ECONNABORTED] = "Connection aborted",
    [ECONNREFUSED] = "Connection refused",
    [ECONNRESET] = "Connection reset",
    [EDEADLK] = "Resource deadlock would occur",
    [EDESTADDRREQ] = "Destination address required",
    [EDOM] = "Mathematics argument out of domain of function",
    [EDQUOT] = "Disk quota exceeded",
    [EEXIST] = "File exists",
    [EFAULT] = "Bad address",
    [EFBIG] = "File too large",
    [EHOSTUNREACH] = "Host is unreachable",
    [EIDRM] = "Identifier removed",
    [EILSEQ] = "Illegal byte sequence",
    [EINPROGRESS] = "Operation in progress",
    [EINTR] = "Interrupted",
    [EINVAL] = "Invalid argument",
    [EIO] = "Input/output error",
    [EISCONN] = "Socket is already connected",
    [EISDIR] = "Is a directory",
    [ELOOP] = "Too many levels of symbolic links",
    [EMFILE] = "Too many open files",
    [EMLINK] = "Too many links",
    [EMSGSIZE] = "Message too large",
    [EMULTIHOP] = "Reserved",
    [ENAMETOOLONG] = "File name too long",
    [ENETDOWN] = "Network is down",
    [ENETRESET] = "Connection aborted by network",
    [ENETUNREACH] = "Network unreachable",
    [ENFILE] = "Too many files open in system",
    [ENOBUFS] = "No buffer space available",
    [ENODEV] = "No such device",
    [ENOENT] = "No such file or directory",
    [ENOEXEC] = "Exec format error",
    [ENOLCK] = "No locks available",
    [ENOMEM] = "Not enough memory",
    [ENOMSG] = "No message of the desired type",
    [ENOPROTOOPT] = "Protocol not available",
    [ENOSPC] = "No space left on device",
    [ENOSYS] = "Functionality not supported",
    [ENOTCONN] = "Socket is not connected",
    [ENOTDIR] = "Not a directory",
    [ENOTEMPTY] = "Directory not empty",
    [ENOTRECOVERABLE] = "State not recoverable",
    [ENOTSOCK] = "Not a socket",
    [ENOTSUP] = "Not supported",
    [ENOTTY] = "Inappropriate I/O control operation",
    [ENXIO] = "No such device or address",
    [EOVERFLOW] = "Value too large for defined data type",
    [EOWNERDEAD] = "Previous owner died",
    [EPERM] = "Operation not permitted",
    [EPIPE] = "Broken pipe",
    [EPROTO] = "Protocol error",
    [EPROTONOSUPPORT] = "Protocol not supported",
    [EPROTOTYPE] = "Protocol wrong type for socket",
    [ERANGE] = "Result out of range",
    [EROFS] = "Read-only file system",
    [ESPIPE] = "Invalid seek",
    [ESRCH] = "No such process",
    [ESTALE] = "Reserved",
    [ETIMEDOUT] = "Connection timed out",
    [ETXTBSY] = "Text file busy",
    [EWOULDBLOCK] = "Operation would block",
    [EXDEV] = "Cross-device link",
};

// missing const is a mistake in a standard and callers cannot change the string
char *strerror(int errnum)
{
    if (errnum < 0 || errnum >= (int)(sizeof(error_msgs) / sizeof(error_msgs[0]))) {
        errno = EINVAL;
        return "Unknown error";
    }

    return (char *)error_msgs[errnum];
}

int strerror_r(int errnum, char *buf, size_t buflen)
{
    if (errnum < 0 || errnum >= (int)(sizeof(error_msgs) / sizeof(error_msgs[0])))
        return EINVAL;

    const char *msg = error_msgs[errnum];
    strncpy(buf, msg, buflen);

    return 0;
}
