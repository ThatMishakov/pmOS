#ifndef _ERRNO_H
#define _ERRNO_H

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int _get_errno();

#define errno _get_errno()

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _ERRNO_H */