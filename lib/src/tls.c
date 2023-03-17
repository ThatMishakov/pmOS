#include <pmos/tls.h>
#include <stdbool.h>
#include <pmos/memory.h>
#include <string.h>

static TLS_Data * global_tls_data = NULL;

#define max(x, y) (x > y ? x : y)
#define alignup(size, alignment) (size%alignment ? size + (alignment - size%alignment) : size)

void init_uthread(struct uthread * u)
{
    u->self = u;
}

struct uthread * prepare_tls()
{
    bool has_tls = global_tls_data != NULL;

    size_t memsz = has_tls ? global_tls_data->memsz : 0;
    size_t align = has_tls ? global_tls_data->align : 0;

    align = max(align, _Alignof(struct uthread));
    size_t alloc_size = alignup(memsz, align) + sizeof(struct uthread);

    size_t size_all = alignup(alloc_size, 4096);

    mem_request_ret_t res = create_normal_region(0, 0, size_all, PROT_READ|PROT_WRITE);
    if (res.result != SUCCESS)
        return NULL;

    unsigned char * tls = (unsigned char *)res.virt_addr + alignup(memsz, align);

    if (memsz > 0) {
        memcpy(tls - memsz, global_tls_data->data, global_tls_data->filesz);
    }
    
    init_uthread((struct uthread *)tls);

    return (struct uthread *)tls;
}

void init_tls_first_time(TLS_Data * d)
{
    global_tls_data = d;

    struct uthread *u = prepare_tls();
    if (u == NULL)
        return; // TODO: Panic

    set_segment(0, SEGMENT_FS, u);
}