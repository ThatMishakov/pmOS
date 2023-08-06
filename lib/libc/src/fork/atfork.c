#include <stddef.h>
#include "atfork.h"

atfork_fn __static_pre_fork[] = {
    __libc_malloc_pre_fork,
};

atfork_fn __static_post_fork_parent[] = {
    __libc_malloc_post_fork_parent,
};

atfork_fn __static_post_fork_child[] = {
    __libc_malloc_post_fork_child,
};

atfork_fn *__atfork_pre_fork = NULL;
size_t __atfork_pre_fork_count = 0;
size_t __atfork_pre_fork_capacity = 0;

atfork_fn *__atfork_post_fork_parent = NULL;
size_t __atfork_post_fork_parent_count = 0;
size_t __atfork_post_fork_parent_capacity = 0;

atfork_fn *__atfork_post_fork_child = NULL;
size_t __atfork_post_fork_child_count = 0;
size_t __atfork_post_fork_child_capacity = 0;

void __libc_atfork_run(atfork_fn *fns, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        fns[i]();
    }
}

void __libc_pre_fork(void)
{
    __libc_atfork_run(__static_pre_fork, sizeof(__static_pre_fork) / sizeof(atfork_fn));
    __libc_atfork_run(__atfork_pre_fork, __atfork_pre_fork_count);
}

void __libc_post_fork_parent(void)
{
    __libc_atfork_run(__atfork_post_fork_parent, __atfork_post_fork_parent_count);
    __libc_atfork_run(__static_post_fork_parent, sizeof(__static_post_fork_parent) / sizeof(atfork_fn));
}

void __libc_post_fork_child(void)
{
    __libc_atfork_run(__atfork_post_fork_child, __atfork_post_fork_child_count);
    __libc_atfork_run(__static_post_fork_child, sizeof(__static_post_fork_child) / sizeof(atfork_fn));
}