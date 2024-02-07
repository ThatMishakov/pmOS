#ifndef _LIBC_ATFORK_H
#define _LIBC_ATFORK_H

typedef void (*atfork_fn)(void);

void __libc_pre_fork(void);
void __libc_post_fork_parent(void);
void __libc_post_fork_child(void);

void __libc_malloc_pre_fork();
void __libc_malloc_post_fork_parent();
void __libc_malloc_post_fork_child();


void __libc_fs_lock_pre_fork();
void __libc_fs_unlock_post_fork();

#endif // _LIBC_ATFORK_H