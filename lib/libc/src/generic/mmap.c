#include <sys/mman.h>
#include <errno.h>
#include <pmos/memory.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    // TODO: mmap can probably be fully implemented soon!
    (void)fd;
    (void)offset;

    if (!(flags & MAP_ANONYMOUS)) {
        // Not supported
        // (although infrastructure for it is mostly there :) )
        errno = ENOTSUP;
        return MAP_FAILED;
    }

    if (length == 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    // Align length to page size
    size_t aligned_length = (length + 4095) & ~4095UL;

    // TODO: Get rid of magic numbers
    mem_request_ret_t req = create_normal_region(0, addr, aligned_length, (prot&0x07) | (flags & MAP_FIXED ? 0x08 : 0));
    if (req.result != SUCCESS) {
        errno = ENOMEM;
        return MAP_FAILED;
    }

    return (void *)req.virt_addr;
}

// The delete region syscall is not implemented yet
// int munmap(void *addr, size_t length) {
//     mem_request_ret_t req = delete_region((uintptr_t)addr, length);
//     if (req.result != SUCCESS) {
//         errno = EINVAL;
//         return -1;
//     }

//     return 0;
// }

int munmap(void *addr, size_t length) {
    (void)addr;
    (void)length;

    // Not supported
    errno = ENOTSUP;
    return -1;
}