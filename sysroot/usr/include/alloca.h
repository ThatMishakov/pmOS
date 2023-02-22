#ifndef _DLFCN_H
#define _DLFCN_H

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef __STDC_HOSTED__

#define alloca(x) __builtin_alloca (x)

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _DLFCN_H */