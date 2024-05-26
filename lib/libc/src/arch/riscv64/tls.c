#include <assert.h>
#include <pmos/memory.h>
#include <pmos/memory_flags.h>
#include <pmos/tls.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

void __init_uthread(struct uthread *u, void *stack_top, size_t stack_size);

#define alignup(size, alignment) (size % alignment ? size + (alignment - size % alignment) : size)

struct uthread *__prepare_tls(void *stack_top, size_t stack_size)
{
    bool has_tls = __global_tls_data != NULL;

    size_t memsz  = has_tls ? __global_tls_data->memsz : 0;
    size_t align  = has_tls ? __global_tls_data->align : 0;
    size_t filesz = has_tls ? __global_tls_data->filesz : 0;
    assert(memsz >= filesz && "TLS memsz must be greater than or equal to TLS filesz");

    size_t alloc_start = alignup(sizeof(struct uthread), align);
    size_t alloc_size  = alloc_start + memsz;

    size_t size_all = alignup(alloc_size, 4096);

    mem_request_ret_t res = create_normal_region(0, 0, size_all, PROT_READ | PROT_WRITE);
    if (res.result != SUCCESS)
        return NULL;

    unsigned char *tls = (unsigned char *)res.virt_addr;

    if (memsz > 0) {
        memcpy(tls + alloc_start, __global_tls_data->data, filesz);
    }

    __init_uthread((struct uthread *)tls, stack_top, stack_size);

    return (struct uthread *)tls;
}

extern int pthread_list_spinlock;
// Worker thread is the list head
extern struct uthread *worker_thread;

void __release_tls(struct uthread *u)
{
    if (u == NULL)
        return;

    pthread_spin_lock(&pthread_list_spinlock);
    u->prev->next = u->next;
    u->next->prev = u->prev;
    pthread_spin_unlock(&pthread_list_spinlock);

    release_region(TASK_ID_SELF, u);
}

struct uthread *__get_tls()
{
    register char *tls asm("tp");

    size_t align       = __global_tls_data ? __global_tls_data->align : 0;
    size_t alloc_start = alignup(sizeof(struct uthread), align);

    return (struct uthread *)(tls - alloc_start);
}

void *__thread_pointer_from_uthread(struct uthread *uthread)
{
    size_t align       = __global_tls_data ? __global_tls_data->align : 0;
    size_t alloc_start = alignup(sizeof(struct uthread), align);

    return (void *)((char *)uthread + alloc_start);
}