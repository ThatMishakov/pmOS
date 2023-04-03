#ifndef _MMAN_H
#define _MMAN_H

#include "../pmos/memory_flags.h"
#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define MAP_FAILED    ((void*)0)

#define POSIX_MADV_NORMAL     0
#define POSIX_MADV_SEQUENTIAL 1
#define POSIX_MADV_RANDOM     2
#define POSIX_MADV_WILLNEED   3
#define POSIX_MADV_DONTNEED   4

#define POSIX_TYPED_MEM_ALLOCATE        0x01
#define POSIX_TYPED_MEM_ALLOCATE_CONTIG 0x02
#define POSIX_TYPED_MEM_MAP_ALLOCATABLE 0x04

static const size_t  posix_tmi_length = -1;
typedef size_t off_t;

#ifdef __STDC_HOSTED__


int    mlock(const void *, size_t);

int    mlockall(int);

void  *mmap(void *, size_t, int, int, int, off_t);
int    munmap(void *, size_t);

int    mprotect(void *, size_t, int);

int    msync(void *, size_t, int);

int    munlock(const void *, size_t);

int    munlockall(void);

int    posix_madvise(void *, size_t, int);

int    posix_mem_offset(const void *restrict, size_t, off_t *restrict,
           size_t *restrict, int *restrict);
int    posix_typed_mem_get_info(int, struct posix_typed_mem_info *);
int    posix_typed_mem_open(const char *, int, int);

int    shm_open(const char *, int, mode_t);
int    shm_unlink(const char *);

#endif /* __STDC_HOSTED__ */

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* _MMAN_H */