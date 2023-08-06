#include <stdint.h>
#ifndef FORK_H
#define FORK_H

struct fork_for_child {
    uint64_t rbp;
    uint64_t thread_loc;
};

#endif // FORK_H