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

struct TaskDescriptor;

struct Generic_Port {
    virtual ~Generic_Port() noexcept = default;
};

struct Message {
    klib::weak_ptr<TaskDescriptor> task_from;
    u64 pid_from = 0;
    klib::vector<char> content;

    inline size_t size() const
    {
        return content.size();
    }

    // Returns true if done successfully, false otherwise (e.g. when syscall needs to be repeated)
    bool copy_to_user_buff(char* buff);
};

#define MSG_ATTR_PRESENT   0x01ULL
#define MSG_ATTR_DUMMY     0x02ULL
#define MSG_ATTR_NODEFAULT 0x03ULL

class TaskGroup;

class Port: public Generic_Port {
public:
    klib::weak_ptr<TaskDescriptor> owner;
    klib::weak_ptr<Port> self;

    Spinlock lock;
    u64 portno;

    Port(const klib::shared_ptr<TaskDescriptor>& owner, u64 portno): owner(owner), portno(portno) {}


    void enqueue(const klib::shared_ptr<Message>& msg);

    void send_from_system(klib::vector<char>&& msg);
    void send_from_system(const char* msg, size_t size);

    // Returns true if successfully sent, false otherwise (e.g. when it is needed to repeat the syscall). Throws on crytical errors
    bool send_from_user(const klib::shared_ptr<TaskDescriptor>& sender, const char *unsafe_user_ptr, size_t msg_size);
    void atomic_send_from_system(const char* msg, size_t size);

    // Returns true if successfully sent, false otherwise (e.g. when it is needed to repeat the syscall). Throws on crytical errors
    bool atomic_send_from_user(const klib::shared_ptr<TaskDescriptor>& sender, const char* unsafe_user_message, size_t msg_size);

    void change_return_upon_unblock(const klib::shared_ptr<TaskDescriptor>& task);

    klib::shared_ptr<Message>& get_front();
    void pop_front() noexcept;
    bool is_empty() const noexcept;

    /**
     * @brief Destructor of port
     * 
     * This destructor cleans up the port and does other misc stuff such as sending not-delivered acknowledgements
     */
    virtual ~Port() noexcept override;
protected:
    using Message_storage = klib::list<klib::shared_ptr<Message>>;
    Message_storage msg_queue;

    klib::splay_tree_map<u64, klib::weak_ptr<TaskGroup>> notifier_ports;
    mutable Spinlock notifier_ports_lock;

    friend class TaskGroup;
};

struct Ports_storage {
    u64 biggest_port = 1;

    klib::splay_tree_map<u64, klib::shared_ptr<Port>> storage;
    Spinlock lock;

    // Atomically gets the port or null if it doesn't exist
    klib::shared_ptr<Port> atomic_get_port(u64 portno);

    // Atomically gets the port. Throws ERROR_PORT_DOESNT_EXIST if there is no port
    klib::shared_ptr<Port> atomic_get_port_throw(u64 portno);

    // Atomically creates a new port with *task* as its owner
    u64 atomic_request_port(const klib::shared_ptr<TaskDescriptor>& task);

    inline bool exists_port(u64 port)
    {
        Auto_Lock_Scope scope_lock(lock);
        return this->storage.count(port) == 1;
    }
};

extern Ports_storage global_ports;