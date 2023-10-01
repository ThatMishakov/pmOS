#include <stdio.h>
#include <stdlib.h>
#include <stdio_internal.h>
#include <pmos/tls.h>
#include <stdbool.h>
#include <pmos/memory.h>
#include <pmos/load_data.h>
#include <string.h>

static TLS_Data * global_tls_data = NULL;

#define max(x, y) (x > y ? x : y)
#define alignup(size, alignment) (size%alignment ? size + (alignment - size%alignment) : size)

void init_uthread(struct uthread * u, void * stack_top, size_t stack_size)
{
    u->self = u;
    u->stack_top = stack_top;
    u->stack_size = stack_size;
}

struct uthread * prepare_tls(void * stack_top, size_t stack_size)
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
    
    init_uthread((struct uthread *)tls, stack_top, stack_size);

    return (struct uthread *)tls;
}

void init_tls_first_time(void * load_data, size_t load_data_size, TLS_Data * d)
{
    global_tls_data = d;

    struct load_tag_stack_descriptor * s = (struct load_tag_stack_descriptor *)get_load_tag(LOAD_TAG_STACK_DESCRIPTOR, load_data, load_data_size);
    if (d == NULL)
        return; // TODO: Panic

    struct uthread *u = prepare_tls((void *)s->stack_top, s->stack_size);
    if (u == NULL)
        return; // TODO: Panic

    set_segment(0, SEGMENT_FS, u);
}

/// @brief Initializes the standard library
///
/// This function initializes the standard library and is the first thing called in _start function (in crt0.S), before
/// calling the constructors and main(). It is responsible for initializing the TLS, file descriptors, arguments and
/// environment variables, since it is not managed by kernel. The dtaa is passed by the caller as a memory region
/// mapped to the process's address space, containing various load_tags containing the descriptors.
/// @param load_data Pointer to the memory region containing the load tags
/// @param load_data_size Size of the memory region containing the load tags
/// @param d Page containing the TLS data
void init_std_lib(void * load_data, size_t load_data_size, TLS_Data * d)
{
    init_tls_first_time(load_data, load_data_size, d);
}

struct load_tag_generic * get_load_tag(uint32_t tag, void * load_data, size_t load_data_size)
{
    struct load_tag_generic * t = load_data;
    while (t->tag != LOAD_TAG_CLOSE) {
        if (t->tag == tag)
            return t;
        t = (struct load_tag_generic *)((unsigned char *)t + t->offset_to_next);
        if ((unsigned char *)t >= (unsigned char *)load_data + load_data_size)
            return NULL;
    }
    return NULL;
}