#ifndef _TLS_H
#define _TLS_H 1
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct TLS_Data {
    uint64_t memsz;
    uint64_t align;

    uint64_t filesz;
    unsigned char data[0];
} TLS_Data;

typedef struct uthread {
    struct uthread *self;
};

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif // _TLS_H