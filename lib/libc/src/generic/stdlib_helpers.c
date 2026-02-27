/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <pmos/ipc.h>
#include <pmos/load_data.h>
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <pmos/tls.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_internal.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/elf.h>
#include <pthread.h>

#define TLS_MODEL0 0
#define TLS_MODEL1 1

#define TLS_MODEL TLS_MODEL1

#define max(x, y)                (x > y ? x : y)
#define alignup(size, alignment) (size % alignment ? size + (alignment - size % alignment) : size)

struct uthread *__get_tls();
void __release_tls(struct uthread *u);
struct uthread *__prepare_tls(void *stack_top, size_t stack_size);

void __init_uthread(struct uthread *u, void *stack_top, size_t stack_size)
{
    u->self             = u;
    u->stack_top        = stack_top;
    u->stack_size       = stack_size;
    u->return_value     = NULL;
    u->atexit_list_head = NULL;
    u->thread_status    = __THREAD_STATUS_RUNNING;
    u->join_notify_port = INVALID_PORT;
    u->next             = u;
    u->prev             = u;
}

void __thread_exit_destroy_tls()
{
    struct uthread *u = __get_tls();

    if (u->thread_status == __THREAD_STATUS_RUNNING) {
        bool not_detached = __sync_bool_compare_and_swap(&u->thread_status, __THREAD_STATUS_RUNNING,
                                                         __THREAD_STATUS_FINISHED);
        if (not_detached)
            // Let caller get the return value and release the TLS
            // If pthread_detach is called at this point, it would be what does it
            return;
    }

    switch (u->thread_status) {
    case __THREAD_STATUS_DETACHED:
        __release_tls(u);
        break;
    case __THREAD_STATUS_JOINING: {
        IPC_Thread_Finished msg = {
            .type      = IPC_Thread_Finished_NUM,
            .flags     = 0,
            .thread_id = (uint64_t)u->self,
        };

        // If it fails there is nothing that can be done
        // Ideally, kernel should guarantee that once the port is created, sending
        // messages will always succeed
        send_message_port(u->join_notify_port, sizeof(msg), (char *)&msg);
        break;
    }
    default:
        fprintf(stderr, "pmOS libC: Invalid thread status: %i\n", u->thread_status);
        assert(false && "Invalid thread status");
        break;
    }
}

void __libc_init_hook() __attribute__((weak));

// TODO: This does not look good
void *__load_data_kernel       = 0;
size_t __load_data_size_kernel = 0;

void *__load_data_user       = 0;
size_t __load_data_size_user = 0;

static void prepare_load_tags(void *load_data, size_t load_data_size)
{
    __load_data_kernel      = load_data;
    __load_data_size_kernel = load_data_size;

    struct load_tag_userspace_tags *t = (struct load_tag_userspace_tags *)get_load_tag(
        LOAD_TAG_USERSPACE_TAGS, load_data, load_data_size);
    if (t) {
        __load_data_user      = (void *)t->tags_address;
        __load_data_size_user = t->memory_size; // This is the size of the page
    }
}

static uint64_t tls_mem_object = 0;
static size_t tls_memsz = 0;
static size_t tls_filesz = 0;
static uintptr_t tls_file_offset = 0;
static uintptr_t tls_page_offset = 0;

extern int pthread_list_spinlock;
// Worker thread is the list head
extern struct uthread *worker_thread;

struct uthread *__prepare_tls(void *stack_top, size_t stack_size)
{
    // TODO: maybe not everything has TLS? :)
    // bool has_tls = tls_mem_object != 0;

    #if TLS_MODEL == TLS_MODEL1
    uintptr_t tls_end = tls_page_offset + tls_memsz + sizeof(struct uthread);
    tls_end = alignup(tls_end, _Alignof(struct uthread));

    size_t size_to_page = alignup(tls_end, PAGE_SIZE);

    map_mem_object_param_t params = {
        .page_table_id = PAGE_TABLE_SELF,
        .object_id = tls_mem_object,
        .addr_start_uint = 0,
        .size = size_to_page,
        .offset_object = tls_file_offset,
        .offset_start = tls_page_offset,
        .object_size = tls_filesz,
        .access_flags = CREATE_FLAG_COW | PROT_READ | PROT_WRITE,
    };

    mem_request_ret_t res = map_mem_object(&params);
    if (res.result != SUCCESS)
        return NULL;

    unsigned char *tls = (unsigned char *)res.virt_addr + tls_page_offset + tls_memsz;

    // #else ...
    #else
    #endif

    __init_uthread((struct uthread *)tls, stack_top, stack_size);

    return (struct uthread *)tls;
}

#if TLS_MODEL == TLS_MODEL1
void __release_tls(struct uthread *u)
{
    if (u == NULL)
        return;

    pthread_spin_lock(&pthread_list_spinlock);
    u->prev->next = u->next;
    u->next->prev = u->prev;
    pthread_spin_unlock(&pthread_list_spinlock);


    release_region(TASK_ID_SELF, (char *)u - tls_file_offset - tls_memsz);
}

void *__get_tp()
{
    if (tls_memsz == 0)
        return NULL;

    return (char *)__get_tls() - tls_memsz;
}

void *__thread_pointer_from_uthread(struct uthread *uthread) { return uthread; }
#endif

static void parse_tls_elf(void *load_data, size_t load_data_size)
{
    struct load_tag_elf_phdr *phdr = (void *)get_load_tag(
        LOAD_TAG_ELF_PHDR, load_data, load_data_size);
    if (!phdr) {
        return;
    }

    struct load_tag_mem_object_id *mo = (void *)get_load_tag(
        LOAD_TAG_MEM_OBJECT_ID, load_data, load_data_size);
    if (!mo) {
        return;
    }

    if (phdr->phdr_size == sizeof(ELF_PHeader_32)) {
        ELF_PHeader_32 *header = (void *)phdr->phdr_addr;
        for (uint64_t i = 0; i < phdr->phdr_num; ++i) {
            ELF_PHeader_32 *e = header + i;
            if (e->type == PT_TLS) {
                tls_memsz = e->p_memsz;
                tls_filesz = e->p_filesz;
                tls_file_offset = e->p_offset;
                tls_page_offset = tls_file_offset % PAGE_SIZE;

                break;
            }
        }
    } else {
        ELF_PHeader_64 *header = (void *)phdr->phdr_addr;
        for (uint64_t i = 0; i < phdr->phdr_num; ++i) {
            ELF_PHeader_64 *e = header + i;
            if (e->type == PT_TLS) {
                tls_memsz = e->p_memsz;
                tls_filesz = e->p_filesz;
                tls_file_offset = e->p_offset;
                tls_page_offset = tls_file_offset % PAGE_SIZE;

                break;
            }
        }
    }

    tls_mem_object = mo->memory_object_id;
}

static void init_tls_first_time(void *load_data, size_t load_data_size)
{
    parse_tls_elf(load_data, load_data_size);

    struct load_tag_stack_descriptor *s = (struct load_tag_stack_descriptor *)get_load_tag(
        LOAD_TAG_STACK_DESCRIPTOR, load_data, load_data_size);
    if (s == NULL)
        return; // TODO: Panic

    struct uthread *u = __prepare_tls((void *)s->stack_top, s->stack_size);
    if (u == NULL)
        return; // TODO: Panic

    void *thread_pointer = __thread_pointer_from_uthread(u);
    #ifdef __i386__
    set_registers(0, SEGMENT_GS, thread_pointer);
    #else
    set_registers(0, SEGMENT_FS, thread_pointer);
    #endif

    if (__libc_init_hook)
        __libc_init_hook();
}

void __init_stdio();

// defined in pthread/threads.c
extern uint64_t __active_threads;

#ifdef __riscv
long __get_gp();
long __libc_gp = 0;
#endif

void __libc_init_dyn();

/// @brief Initializes the standard library
///
/// This function initializes the standard library and is the first thing called in _start function
/// (in crt0.S), before calling the constructors and main(). It is responsible for initializing the
/// TLS, file descriptors, arguments and environment variables, since it is not managed by kernel.
/// The dtaa is passed by the caller as a memory region mapped to the process's address space,
/// containing various load_tags containing the descriptors.
/// @param load_data Pointer to the memory region containing the load tags
/// @param load_data_size Size of the memory region containing the load tags
/// @param d Page containing the TLS data
void init_std_lib(void *load_data, size_t load_data_size)
{
    #ifdef __riscv
    __libc_gp = __get_gp();
    #endif

    prepare_load_tags(load_data, load_data_size);
    init_tls_first_time(load_data, load_data_size);

    __active_threads = 1;

    __init_stdio();

    __libc_init_dyn();
}

struct load_tag_generic *get_load_tag(uint32_t tag, void *load_data, size_t load_data_size)
{
    struct load_tag_generic *t = load_data;
    while (t->tag != LOAD_TAG_CLOSE) {
        if (t->tag == tag)
            return t;
        t = (struct load_tag_generic *)((unsigned char *)t + t->offset_to_next);
        if ((unsigned char *)t >= (unsigned char *)load_data + load_data_size)
            return NULL;
    }
    return NULL;
}

struct load_tag_generic *get_load_tag_global(uint32_t tag, size_t index)
{
    struct load_tag_generic *t = __load_data_kernel;
    while (t->tag != LOAD_TAG_CLOSE) {
        if (t->tag == tag) {
            if (index == 0)
                return t;
            index--;
        }
        t = (struct load_tag_generic *)((unsigned char *)t + t->offset_to_next);
        if ((unsigned char *)t >= (unsigned char *)__load_data_kernel + __load_data_size_kernel)
            return NULL;
    }
    
    if (!__load_data_user || !__load_data_size_user)
        return NULL;
    
    uintptr_t base = (uintptr_t)__load_data_user;
    uintptr_t end  = base + __load_data_size_user;

    uintptr_t p = base;
    while (p + sizeof(struct load_tag_generic) <= end) {
        struct load_tag_generic *t = (struct load_tag_generic *)p;
        if (t->tag == LOAD_TAG_CLOSE)
            break;

        if (p + t->offset_to_next > end)
            break; // Out of bounds

        if (t->tag == tag) {
            if (index == 0)
                return t;
            index--;
        }

        if (t->offset_to_next == 0)
            break; // infinite loop
        if (t->offset_to_next < sizeof(struct load_tag_generic))
            break; // invalid offset

        p += t->offset_to_next;
    }
    return NULL;
}