#ifndef _DLFCN_H
#define _DLFCN_H

#if defined(__cplusplus)
extern "C" {
#endif

#define RTLD_LAZY 0x00
#define RTLD_NOW  0x01

#define RTLD_GLOBAL 0x10
#define RTLD_LOCAL  0x20

int dlclose(void *);
char *dlerror(void);
void *dlopen(const char *, int);
void *dlsym(void *, const char *);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _DLFCN_H */