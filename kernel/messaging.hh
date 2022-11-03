#pragma once
#include <stdint.h>
#include "lib/list.hh"
#include "lib/queue.hh"
#include "utils.hh"
#include "lib/vector.hh"
#include "types.hh"

struct Message {
    uint64_t from;
    klib::vector<char> content;

    inline size_t size() const
    {
        return content.size();
    }
};

using Message_storage = klib::queue<Message>;

struct TaskDescriptor;

kresult_t queue_message(TaskDescriptor* task, uint64_t from, char* message_usr_ptr, size_t size);