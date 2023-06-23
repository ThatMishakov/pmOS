#include "../include/string.h"
#include <stdlib.h>
#include <errno.h>

size_t strlen(const char* str)
{
    size_t size = 0;
    for(;str[size] != '\0'; ++size);

    return size;
}

char * strncpy ( char * destination, const char * source, size_t num )
{
    char* p = destination;
    
    while (num > 0 && *source != '\0') {
        *p = *source;
        ++p; ++source;
        --num;
    }

    while (num-- > 0) {
        *p = '\0';
        ++p;
    }

    return destination;
}

int strcmp(const char *s1, const char *s2)
{
    size_t i = 0;
    while (s1[i] == s2[i] && s1[i]) ++i;

    if (!s1[i] && !s2[i]) return 0;
    if (s1[i] < s2[i]) return -1;
    return 1; 
}

int strncmp(const char *s1, const char *s2, size_t size)
{
    size_t i = 0;
    while (i < size && s1[i] == s2[i] && s1[i]) ++i;

    if (i == size || !s1[i] && !s2[i]) return 0;
    if (s1[i] < s2[i]) return -1;
    return 1; 
}

void *memcpy(void *dest, const void * src, size_t n)
{
    void* k = dest;
    while (n--) {
        *((char*)dest++) = *((char*)src++);
    }

    return k;
}

void *memset(void *s, int c, size_t n)
{
    void *k = s;
    while (n--) {
        *((unsigned char*)(s)++) = (unsigned char)c;
    }

    return k;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    size_t k = 0;
    for (; k < n && ((unsigned char*)s1)[k] == ((unsigned char*)s2)[k]; ++k) ;

    if (k == n) return 0;

    return ((unsigned char*)s1)[k] < ((unsigned char*)s2)[k] ? -1 : 1; 
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
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

char *strchr(const char *str, int c) {
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

char *strrchr(const char *str, int c) {
    char ch = (char)c;
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

char *strdup(const char *str) {
    size_t length = strlen(str) + 1;
    char *duplicate = malloc(length);

    if (duplicate != NULL) {
        memcpy(duplicate, str, length);
    }

    return duplicate;
}

size_t strnlen(const char *str, size_t maxlen) {
    size_t length = 0;

    while (length < maxlen && str[length] != '\0') {
        length++;
    }

    return length;
}

const char *strerror(int errnum) {
    switch (errnum) {
        case 0:
            return "No error";
        case E2BIG:
            return "Argument list too long";
        case EACCES:
            return "Permission denied";
        case EADDRINUSE:
            return "Address already in use";
        case EADDRNOTAVAIL:
            return "Cannot assign requested address";
        case EAFNOSUPPORT:
            return "Address family not supported";
        case EAGAIN:
            return "Resource temporarily unavailable";
        case EALREADY:
            return "Operation already in progress";
        case EBADF:
            return "Bad file descriptor";
        case EBADMSG:
            return "Bad message";
        case EBUSY:
            return "Device or resource busy";
        case ECANCELED:
            return "Operation canceled";
        case ECHILD:
            return "No child processes";
        case ECONNABORTED:
            return "Connection aborted";
        case ECONNREFUSED:
            return "Connection refused";
        case ECONNRESET:
            return "Connection reset";
        case EDEADLK:
            return "Resource deadlock would occur";
        case EDESTADDRREQ:
            return "Destination address required";
        case EDOM:
            return "Mathematics argument out of domain of function";
        case EDQUOT:
            return "Disk quota exceeded";
        case EEXIST:
            return "File exists";
        case EFAULT:
            return "Bad address";
        case EFBIG:
            return "File too large";
        case EHOSTUNREACH:
            return "Host is unreachable";
        case EIDRM:
            return "Identifier removed";
        case EILSEQ:
            return "Illegal byte sequence";
        case EINPROGRESS:
            return "Operation in progress";
        case EINTR:
            return "Interrupted function";
        case EINVAL:
            return "Invalid argument";
        case EIO:
            return "Input/output error";
        case EISCONN:
            return "Socket is already connected";
        case EISDIR:
            return "Is a directory";
        case ELOOP:
            return "Too many levels of symbolic links";
        case EMFILE:
            return "Too many open files";
        case EMLINK:
            return "Too many links";
        case EMSGSIZE:
            return "Message too large";
        case EMULTIHOP:
            return "Reserved";
        case ENAMETOOLONG:
            return "File name too long";
        case ENETDOWN:
            return "Network is down";
        case ENETRESET:
            return "Connection aborted by network";
        case ENETUNREACH:
            return "Network unreachable";
        case ENFILE:
            return "Too many files open in system";
        case ENOBUFS:
            return "No buffer space available";
        case ENODEV:
            return "No such device";
        case ENOENT:
            return "No such file or directory";
        case ENOEXEC:
            return "Exec format error";
        case ENOLCK:
            return "No locks available";
        case ENOMEM:
            return "Not enough memory";
        case ENOMSG:
            return "No message of the desired type";
        case ENOPROTOOPT:
            return "Protocol not available";
        case ENOSPC:
            return "No space left on device";
        case ENOSYS:
            return "Functionality not supported";
        case ENOTCONN:
            return "Socket is not connected";
        case ENOTDIR:
            return "Not a directory";
        case ENOTEMPTY:
            return "Directory not empty";
        case ENOTRECOVERABLE:
            return "State not recoverable";
        case ENOTSOCK:
            return "Not a socket";
        case ENOTSUP:
            return "Not supported";
        case ENOTTY:
            return "Inappropriate I/O control operation";
        case ENXIO:
            return "No such device or address";
        case EOPNOTSUPP:
            return "Operation not supported on socket";
        case EOVERFLOW:
            return "Value too large for defined data type";
        case EOWNERDEAD:
            return "Previous owner died";
        case EPERM:
            return "Operation not permitted";
        case EPIPE:
            return "Broken pipe";
        case EPROTO:
            return "Protocol error";
        case EPROTONOSUPPORT:
            return "Protocol not supported";
        case EPROTOTYPE:
            return "Protocol wrong type for socket";
        case ERANGE:
            return "Result out of range";
        case EROFS:
            return "Read-only file system";
        case ESPIPE:
            return "Invalid seek";
        case ESRCH:
            return "No such process";
        case ESTALE:
            return "Reserved";
        case ETIMEDOUT:
            return "Connection timed out";
        case ETXTBSY:
            return "Text file busy";
        case EWOULDBLOCK:
            return "Operation would block";
        case EXDEV:
            return "Cross-device link";
        default:
            return "Unknown error";
    }
}
