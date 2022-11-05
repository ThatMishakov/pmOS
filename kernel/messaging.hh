#pragma once
#include <stdint.h>
#include "lib/list.hh"
#include "lib/queue.hh"
#include "utils.hh"
#include "lib/vector.hh"
#include "types.hh"
#include "lib/splay_tree_map.hh"

struct Message {
    uint64_t from;
    uint64_t channel;
    klib::vector<char> content;

    inline size_t size() const
    {
        return content.size();
    }

    kresult_t copy_to_user_buff(char* buff);
};

struct Port_Desc {
    uint64_t task;
    uint64_t channel;
};

using Message_storage = klib::queue<Message>;
using Ports_storage = klib::splay_tree_map<uint64_t, Port_Desc>;

struct TaskDescriptor;

kresult_t queue_message(TaskDescriptor* task, uint64_t from, uint64_t channel, char* message_usr_ptr, size_t size);