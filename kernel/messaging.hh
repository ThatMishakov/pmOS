#pragma once
#include <stdint.h>
#include "lib/list.hh"
#include "lib/queue.hh"
#include "utils.hh"

struct Message {
    uint64_t from;
    uint64_t content;
    uint64_t ptr;

    Message()
    {
        t_print("Created Message\n");
    }

    ~Message()
    {
        t_print("Deleted Message\n");
    }
};

using Message_storage = Queue<Message>;