#ifndef _ERRNO_H
#define _ERRNO_H

#if defined(__cplusplus)
extern "C" {
#endif

int _get_errno();

#define errno _get_errno()

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _ERRNO_H */