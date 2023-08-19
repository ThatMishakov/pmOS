#include <stdint.h>
#include "../filesystem.h"
#ifndef FORK_H
#define FORK_H

struct Filesystem_Data;
struct fork_for_child {
    uint64_t rbp;
    uint64_t thread_loc;
    struct Filesystem_Data * fs_data;
};

void __libc_fixup_fs_post_fork(struct fork_for_child * child_data);

#endif // FORK_H