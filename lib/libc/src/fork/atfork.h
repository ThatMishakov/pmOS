#ifndef _LIBC_ATFORK_H
#define _LIBC_ATFORK_H

typedef void (*atfork_fn)(void);

void __libc_pre_fork(void);
void __libc_post_fork_parent(void);
void __libc_post_fork_child(void);

void __libc_malloc_pre_fork();
void __libc_malloc_post_fork_parent();
void __libc_malloc_post_fork_child();

#endif // _LIBC_ATFORK_H