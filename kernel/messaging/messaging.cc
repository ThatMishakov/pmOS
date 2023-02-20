#include "messaging.hh"
#include <types.hh>
#include <sched/sched.hh>
#include <stddef.h>
#include <kernel/errors.h>
#include <lib/utility.hh>
#include <kernel/block.h>
#include <utils.hh>
#include <processes/syscalls.hh>

Ports_storage global_ports;
Spinlock messaging_ports;

kresult_t Message::copy_to_user_buff(char* buff)
{
    return copy_to_user(&content.front(), buff, content.size());
}


// kresult_t Ports_storage::set_port(u64 port,  const klib::shared_ptr<TaskDescriptor>& t, u64 dest_chan)
// {
//     if (exists_port(port)) {
//         lock.lock();
//         Port& p = *this->storage.at(port);

//         p.task = t->pid;;
//         p.channel = dest_chan;
//         p.attr &= ~MSG_ATTR_DUMMY;
//         p.attr |= MSG_ATTR_PRESENT;

//         if (not p.msg_queue.empty()) {

//             Auto_Lock_Scope messaging_lock(t->messaging_lock);
//             while (not p.msg_queue.empty()) {
//                 klib::shared_ptr<Message> m = klib::move(p.msg_queue.front());
//                 p.msg_queue.pop_front();

//                 m->channel = dest_chan;
//                 t->messages.push_back(klib::move(m));
//             }

//             unblock_if_needed(t, MESSAGE_S_NUM, 0);
//         }
//         lock.unlock();
//     }

//     this->lock.lock();
//     this->storage.insert({port, klib::make_shared<Port>(Port({t->pid, dest_chan, MSG_ATTR_PRESENT, {}, {}, port}))});
//     this->lock.unlock();

//     return SUCCESS;
// }

ReturnStr<u64> Ports_storage::atomic_request_port(const klib::shared_ptr<TaskDescriptor>& t)
{
    Auto_Lock_Scope local_lock(lock);

    u64 new_port;
    if (storage.empty()) {
        new_port = biggest_port;
    } else {
        new_port = max(biggest_port, storage.largest() + 1);
    }

    biggest_port = new_port + 1;

    klib::shared_ptr<Port> new_port_ptr = klib::make_shared<Port>(t, new_port);
    new_port_ptr->self = new_port_ptr;

    storage.insert({new_port, new_port_ptr});

    return {SUCCESS, new_port};
}

klib::shared_ptr<Port> Ports_storage::atomic_get_port(u64 portno)
{
    Auto_Lock_Scope scope_lock(lock);

    return storage.get_copy_or_default(portno);
}

kresult_t Port::atomic_send_from_system(const char* msg_ptr, uint64_t size)
{
    Auto_Lock_Scope scope_lock(lock);

    return send_from_system(msg_ptr, size);
}

kresult_t Port::enqueue(const klib::shared_ptr<TaskDescriptor>& from, const klib::shared_ptr<Message>& msg)
{
    klib::shared_ptr<TaskDescriptor> t = owner.lock();

    if (not t)
        return ERROR_GENERAL;

    msg_queue.push_back(msg);
    unblock_if_needed(t, self.lock());

    return SUCCESS;
} 

kresult_t Port::send_from_system(klib::vector<char>&& v)
{
    klib::shared_ptr<TaskDescriptor> task_ptr = nullptr;

    klib::shared_ptr<Message> ptr = klib::make_shared<Message>(task_ptr, 0, klib::forward<klib::vector<char>>(v));

    return enqueue(nullptr, ptr);
}

kresult_t Port::send_from_system(const char* msg_ptr, uint64_t size)
{
    klib::vector<char> message(size);
    memcpy(&message.front(), msg_ptr, size);
    return send_from_system(klib::move(message));
}

kresult_t Port::send_from_user(const klib::shared_ptr<TaskDescriptor>& sender, const char *unsafe_user_ptr, size_t msg_size)
{
    klib::vector<char> message(msg_size);

    kresult_t result = copy_from_user(&message.front(), (char*)unsafe_user_ptr, msg_size);
    if (result != SUCCESS) return result;

    klib::shared_ptr<Message> ptr = klib::make_shared<Message>(sender, sender->pid, klib::forward<klib::vector<char>>(message));

    return enqueue(sender, ptr);
}

kresult_t Port::atomic_send_from_user(const klib::shared_ptr<TaskDescriptor>& sender, const char* unsafe_user_message, size_t msg_size)
{
    klib::vector<char> message(msg_size);

    kresult_t result = copy_from_user(&message.front(), (char*)unsafe_user_message, msg_size);
    if (result != SUCCESS) return result;

    klib::shared_ptr<Message> ptr = klib::make_shared<Message>(sender, sender->pid, klib::forward<klib::vector<char>>(message));

    Auto_Lock_Scope scope_lock(lock);

    return enqueue(sender, ptr);
}