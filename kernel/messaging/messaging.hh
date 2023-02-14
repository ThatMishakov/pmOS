#pragma once
#include <lib/list.hh>
#include <lib/queue.hh>
#include <utils.hh>
#include <lib/vector.hh>
#include <types.hh>
#include <lib/splay_tree_map.hh>
#include <types.hh>
#include <lib/memory.hh>

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

using Message_storage = klib::list<klib::shared_ptr<Message>>;


#define MSG_ATTR_PRESENT   0x01ULL
#define MSG_ATTR_DUMMY     0x02ULL
#define MSG_ATTR_NODEFAULT 0x03ULL
struct Port {
    u64 task = 0;
    u64 channel = 0;
    u64 attr = 0; // NODEFAULT DUMMY PRESENT
    Message_storage msg_queue;
    Spinlock lock;
    u64 portno;


    kresult_t enqueue(u64 from, klib::vector<char>&& msg);
    kresult_t send_from_system(const char* msg, size_t size);
};

struct TaskDescriptor;

struct Ports_storage {
    u64 biggest_port = 1;

    klib::splay_tree_map<u64, klib::shared_ptr<Port>> storage;
    Spinlock lock;
    kresult_t send_from_user(u64 pid_from, u64 port, u64 buff_addr, size_t size);
    kresult_t send_from_system(u64 port, const char* msg, size_t size);
    kresult_t set_dummy(u64 port);
    kresult_t set_port(u64 port, const klib::shared_ptr<TaskDescriptor>& task, u64 dest_chan);
    kresult_t send_msg(u64 pid_from, u64 port, klib::vector<char>&& msg);
    ReturnStr<u64> atomic_request_port(const klib::shared_ptr<TaskDescriptor>& task);
    inline bool exists_port(u64 port)
    {
        Auto_Lock_Scope scope_lock(lock);
        return this->storage.count(port) == 1;
    }

    // Atomically gets the port or null if it doesn't exist
    klib::shared_ptr<Port> atomic_get_port(u64 portno);
};

extern Ports_storage kernel_ports;
extern Ports_storage default_ports;

kresult_t queue_message(const klib::shared_ptr<TaskDescriptor>& task, klib::shared_ptr<Message> message);

kresult_t init_kernel_ports();

// Sends a message from the system
kresult_t send_message_system(u64 port, const char* msg, size_t size);

// Sends a message to the default port
kresult_t send_msg_default(u64 pid_from, u64 port, klib::vector<char>&& msg);