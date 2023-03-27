#ifndef _ERRNO_H
#define _ERRNO_H

#if defined(__cplusplus)
extern "C" {
#endif

#define EINVAL 1

#ifdef __STDC_HOSTED__

/// This variable is used for returning errors in C functions.
/// The variable is defined as weak so if the application does not have thread-local data,
/// it might be freely redefined with the appropriate storage specificator
int __thread __attribute__((weak)) errno = 0;

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _ERRNO_H */