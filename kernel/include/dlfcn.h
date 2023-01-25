#ifndef _DLFCN_H
#define _DLFCN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *dli_fname;
    void       *dli_fbase;
    const char *dli_sname;
    void       *dli_saddr;
} Dl_info;


inline int dladdr(void*, Dl_info*) {
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif