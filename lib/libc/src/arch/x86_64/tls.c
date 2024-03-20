#include <pmos/tls.h>
#include <stdbool.h>
#include <pmos/memory.h>
#include <pmos/memory_flags.h>
#include <assert.h>
#include <string.h>

void __init_uthread(struct uthread * u, void * stack_top, size_t stack_size);

struct uthread * __prepare_tls(void * stack_top, size_t stack_size)
{
    bool has_tls = __global_tls_data != NULL;

    size_t memsz = has_tls ? __global_tls_data->memsz : 0;
    size_t align = has_tls ? __global_tls_data->align : 0;
    size_t filesz = has_tls ? __global_tls_data->filesz : 0;
    assert(memsz >= filesz && "TLS memsz must be greater than or equal to TLS filesz");

    align = max(align, _Alignof(struct uthread));
    size_t memsz_aligned = alignup(memsz, align);
    size_t alloc_size = memsz_aligned + sizeof(struct uthread);

    size_t size_all = alignup(alloc_size, 4096);

    mem_request_ret_t res = create_normal_region(0, 0, size_all, PROT_READ|PROT_WRITE);
    if (res.result != SUCCESS)
        return NULL;

    unsigned char * tls = (unsigned char *)res.virt_addr + memsz_aligned;

    if (memsz > 0) {
        memcpy(tls - memsz_aligned, __global_tls_data->data, filesz);
    }
    
    __init_uthread((struct uthread *)tls, stack_top, stack_size);

    return (struct uthread *)tls;
}

void __release_tls(struct uthread * u)
{
    if (u == NULL)
        return;

    size_t memsz = __global_tls_data->memsz;
    size_t align = __global_tls_data->align;

    align = max(align, _Alignof(struct uthread));
    // size_t alloc_size = alignup(memsz, align) + sizeof(struct uthread);
    // size_t size_all = alignup(alloc_size, 4096);

    release_region(PID_SELF, (void *)u - alignup(memsz, align));
}

void * __thread_pointer_from_uthread(struct uthread * uthread)
{
    return uthread;
}