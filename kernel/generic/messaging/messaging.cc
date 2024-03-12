#include "messaging.hh"
#include <types.hh>
#include <sched/sched.hh>
#include <stddef.h>
#include <kernel/errors.h>
#include <lib/utility.hh>
#include <kernel/block.h>
#include <utils.hh>
#include <processes/syscalls.hh>
#include <assert.h>
#include <processes/task_group.hh>

bool Message::copy_to_user_buff(char* buff)
{
    return copy_to_user(&content.front(), buff, content.size());
}

klib::shared_ptr<Port> Port::atomic_create_port(const klib::shared_ptr<TaskDescriptor>& task)
{
    assert(task);

    u64 new_port = __atomic_add_fetch(&biggest_port, 1, __ATOMIC_SEQ_CST);

    klib::shared_ptr<Port> new_port_ptr = klib::make_shared<Port>(task, new_port);

    {
        Auto_Lock_Scope local_lock(ports_lock);
        ports.insert({new_port, new_port_ptr});
    }

    new_port_ptr->self = new_port_ptr;

    try {
        Auto_Lock_Scope local_lock(task->messaging_lock);
        task->owned_ports.insert(new_port_ptr);
    } catch (...) {
        Auto_Lock_Scope local_lock(ports_lock);
        ports.erase(new_port);
        throw;
    }

    return new_port_ptr;
}

klib::shared_ptr<Port> Port::atomic_get_port(u64 portno) noexcept
{
    Auto_Lock_Scope scope_lock(ports_lock);

    return ports.get_copy_or_default(portno).lock();
}

klib::shared_ptr<Port> Port::atomic_get_port_throw(u64 portno)
{
    Auto_Lock_Scope scope_lock(ports_lock);
    const auto ptr = ports.get_copy_or_default(portno).lock();

    if (not ptr)
        throw (Kern_Exception(ERROR_PORT_DOESNT_EXIST, "requested port does not exist"));

    return ptr;
}

void Port::atomic_send_from_system(const char* msg_ptr, uint64_t size)
{
    Auto_Lock_Scope scope_lock(lock);

    send_from_system(msg_ptr, size);
}

void Port::enqueue(const klib::shared_ptr<Message>& msg)
{
    assert(lock.is_locked() && "Spinlock not locked!");

    klib::shared_ptr<TaskDescriptor> t = owner.lock();

    if (not t)
        throw(Kern_Exception(ERROR_GENERAL, "port owner does not exist"));

    msg_queue.push_back(msg);
    unblock_if_needed(t, self.lock());
} 

void Port::send_from_system(klib::vector<char>&& v)
{
    assert(lock.is_locked() && "Spinlock not locked!");

    klib::shared_ptr<TaskDescriptor> task_ptr = nullptr;

    klib::shared_ptr<Message> ptr = klib::make_shared<Message>(task_ptr, 0, klib::forward<klib::vector<char>>(v));

    enqueue(ptr);
}

void Port::send_from_system(const char* msg_ptr, uint64_t size)
{
    klib::vector<char> message(size);
    memcpy(&message.front(), msg_ptr, size);
    send_from_system(klib::move(message));
}

bool Port::send_from_user(const klib::shared_ptr<TaskDescriptor>& sender, const char *unsafe_user_ptr, size_t msg_size)
{
    assert(lock.is_locked() && "Spinlock not locked!");

    klib::vector<char> message(msg_size);

    kresult_t result = copy_from_user(&message.front(), (char*)unsafe_user_ptr, msg_size);
    if (not result)
        return result;

    klib::shared_ptr<Message> ptr = klib::make_shared<Message>(sender, sender->pid, klib::forward<klib::vector<char>>(message));

    enqueue(ptr);

    return true;
}

bool Port::atomic_send_from_user(const klib::shared_ptr<TaskDescriptor>& sender, const char* unsafe_user_message, size_t msg_size)
{
    klib::vector<char> message(msg_size);

    bool result = copy_from_user(&message.front(), (char*)unsafe_user_message, msg_size);
    if (not result)
        return result;

    klib::shared_ptr<Message> ptr = klib::make_shared<Message>(sender, sender->pid, klib::forward<klib::vector<char>>(message));

    Auto_Lock_Scope scope_lock(lock);

    enqueue(ptr);

    return true;
}

void Port::pop_front() noexcept
{
    assert(lock.is_locked() && "Spinlock not locked!");

    return msg_queue.pop_front();
}

bool Port::is_empty() const noexcept
{
    assert(lock.is_locked() && "Spinlock not locked!");

    return msg_queue.empty();
}

klib::shared_ptr<Message>& Port::get_front()
{
    assert(lock.is_locked() && "Spinlock not locked!");

    return msg_queue.front();
}

Port::~Port() noexcept
{
    for (const auto &p : notifier_ports) {
        const auto &ptr = p.second.lock();
        if (ptr) {
            Auto_Lock_Scope l(ptr->notifier_ports_lock);
            ptr->notifier_ports.erase(portno);
        }
    }

    Auto_Lock_Scope scope_lock(ports_lock);
    ports.erase(portno);
}

Port::Port(const klib::shared_ptr<TaskDescriptor>& owner, u64 portno): owner(owner), portno(portno) {}