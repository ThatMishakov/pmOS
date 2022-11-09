#pragma once
#include <lib/list.hh>
#include <lib/queue.hh>
#include <utils.hh>
#include <lib/vector.hh>
#include <types.hh>
#include <lib/splay_tree_map.hh>

extern Spinlock messaging_ports;

struct Message {
    u64 from;
    u64 channel;
    klib::vector<char> content;

    inline size_t size() const
    {
        return content.size();
    }

    kresult_t copy_to_user_buff(char* buff);
};

using Message_storage = klib::queue<Message>;

struct Port {
    u64 task = 0;
    u64 channel = 0;
    u64 attr = 0;
    Message_storage msg_queue;

    kresult_t enqueue(u64 from, u64 size, const char* buff, bool from_user = true);
};

struct Ports_storage {
    klib::splay_tree_map<u64, Port> storage;
    kresult_t send_from_user(u64 pid_from, u64 port, u64 buff_addr, size_t size);
    kresult_t send_from_system(u64 port, const char* msg, size_t size);
    kresult_t set_dummy(u64 port);
    kresult_t set_port(u64 port, u64 dest_pid, u64 dest_chan);
};

extern Ports_storage* kernel_ports;

struct TaskDescriptor;

kresult_t queue_message(TaskDescriptor* task, u64 from, u64 channel, const char* message_usr_ptr, size_t size, bool from_user = true);

kresult_t init_kernel_ports();

// Sends a message from the system
kresult_t send_message_system(u64 port, const char* msg, size_t size);