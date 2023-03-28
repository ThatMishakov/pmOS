#ifndef _ERRNO_H
#define _ERRNO_H

#if defined(__cplusplus)
extern "C" {
#endif

#define EINVAL 1

#ifdef __STDC_HOSTED__

/// This function returns the pointer to thread-local __errno variable. The program might redefine this function
/// if thread-local storage is not supported.
int* __attribute__((weak)) __get_errno();

#endif

#define errno (*__get_errno())

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _ERRNO_H */