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

using Message_storage = klib::queue<Message>;

struct Port {
    uint64_t task = 0;
    uint64_t channel = 0;
    uint64_t attr = 0;
    Message_storage msg_queue;

    kresult_t enqueue(uint64_t from, uint64_t size, char* buff, bool from_user = true);
};

struct Ports_storage {
    klib::splay_tree_map<uint64_t, Port> storage;
    kresult_t send_from_user(uint64_t pid_from, uint64_t port, uint64_t buff_addr, size_t size);
    kresult_t send_from_system(uint64_t port, char* msg, size_t size);
    kresult_t set_dummy(uint64_t port);
    kresult_t set_port(uint64_t port, uint64_t dest_pid, uint64_t dest_chan);
};

extern Ports_storage* kernel_ports;

struct TaskDescriptor;

kresult_t queue_message(TaskDescriptor* task, uint64_t from, uint64_t channel, char* message_usr_ptr, size_t size, bool from_user = true);

kresult_t init_kernel_ports();