#include "messaging.hh"
#include <types.hh>
#include <processes/sched.hh>
#include <stddef.h>
#include <kernel/errors.h>
#include <lib/utility.hh>
#include <kernel/block.h>
#include <utils.hh>

Ports_storage kernel_ports;
Ports_storage default_ports;
Spinlock messaging_ports;

kresult_t init_kernel_ports()
{
    default_ports.set_dummy(1); // General log messages

    return SUCCESS;
}

kresult_t queue_message(const klib::shared_ptr<TaskDescriptor>& task, klib::shared_ptr<Message> message)
{
    task->messages.emplace_back(klib::move(message));

    return SUCCESS;
}

kresult_t Message::copy_to_user_buff(char* buff)
{
    return copy_to_user(&content.front(), buff, content.size());
}

kresult_t Port::enqueue(u64 from, klib::vector<char>&& msg_v)
{
    klib::shared_ptr<Message> p = klib::make_shared<Message>(from, channel, klib::forward<klib::vector<char>>(msg_v));

    this->lock.lock();
    this->msg_queue.emplace_back(klib::move(p));
    this->lock.unlock();

    return SUCCESS;
}

kresult_t Ports_storage::send_from_user(u64 pid_from, u64 port, u64 buff_addr, size_t size)
{
    klib::vector<char> message(size);

    kresult_t result = copy_from_user((char*)buff_addr, &message.front(), size);
    if (result != SUCCESS) return result;

    return send_msg(pid_from, port, klib::move(message));
}

kresult_t Ports_storage::send_from_system(u64 port, const char* msg, size_t size)
{
    static const u64 pid_from = 0;
    klib::vector<char> message(size);
    memcpy(msg, &message.front(), size);

    return send_msg(pid_from, port, klib::move(message));
}

kresult_t Ports_storage::send_msg(u64 pid_from, u64 port, klib::vector<char>&& msg)
{
    // TODO: Race conditions
    this->lock.lock();
    bool exists_port = this->storage.count(port) == 1;
    this->lock.unlock();

    if (not exists_port) {
        return send_msg_default(pid_from, port, forward<klib::vector<char>>(msg));
    }

    kresult_t result = ERROR_NODESTINATION;
    this->lock.lock();
    Port& d = this->storage.at(port);
    this->lock.unlock();

    klib::shared_ptr<TaskDescriptor> task = get_task(d.task);
    
    if (d.attr&MSG_ATTR_PRESENT and not task){
        d.attr &= ~MSG_ATTR_PRESENT;
        result = ERROR_PORT_CLOSED;
    }

    if (d.attr & MSG_ATTR_PRESENT) {
        result = SUCCESS;

        klib::shared_ptr<Message> ptr = klib::make_shared<Message>(Message({pid_from, d.channel, klib::forward<klib::vector<char>>(msg)}));

        {
            Auto_Lock_Scope messaging_lock(task->messaging_lock);
            result = queue_message(task, ptr);
        }
    } else if (d.attr & MSG_ATTR_DUMMY) {
        return d.enqueue(pid_from, klib::forward<klib::vector<char>>(msg));
    } if (not (d.attr & MSG_ATTR_NODEFAULT)) {
        result = send_msg_default(pid_from, port, forward<klib::vector<char>>(msg));
    }

    return result;
}

kresult_t Ports_storage::set_port(u64 port,  const klib::shared_ptr<TaskDescriptor>& t, u64 dest_chan)
{
    if (exists_port(port)) {
        lock.lock();
        Port& p = this->storage.at(port);

        p.task = t->pid;;
        p.channel = dest_chan;
        p.attr &= ~MSG_ATTR_DUMMY;
        p.attr |= MSG_ATTR_PRESENT;

        if (not p.msg_queue.empty()) {

            Auto_Lock_Scope messaging_lock(t->messaging_lock);
            while (not p.msg_queue.empty()) {
                klib::shared_ptr<Message> m = klib::move(p.msg_queue.front());
                p.msg_queue.pop_front();

                m->channel = dest_chan;
                t->messages.push_back(klib::move(m));
            }

            unblock_if_needed(t, MESSAGE_S_NUM);
        }
        lock.unlock();
    }

    this->lock.lock();
    this->storage.insert({port, {t->pid, dest_chan, MSG_ATTR_PRESENT, {}, {}}});
    this->lock.unlock();

    return SUCCESS;
}

kresult_t Ports_storage::set_dummy(u64 port)
{
    lock.lock();
    if (this->storage.count(port) == 1) {
        this->storage.at(port).attr |= MSG_ATTR_DUMMY;
    } else {
        this->storage.insert({port, {0,0,MSG_ATTR_DUMMY, {}, {}}});
    }
    lock.unlock();

    return SUCCESS;
}

kresult_t send_message_system(u64 port, const char* msg, size_t size)
{
    return kernel_ports.send_from_system(port, msg, size);
}

kresult_t send_msg_default(u64 pid_from, u64 port, klib::vector<char>&& msg)
{
    if (default_ports.storage.count(port) == 0) {
        return ERROR_PORT_NOT_EXISTS;
    }

    kresult_t result = ERROR_NODESTINATION;

    default_ports.lock.lock();
    Port& d = default_ports.storage.at(port);
    default_ports.lock.unlock();

    klib::shared_ptr<TaskDescriptor> task = get_task(d.task);
    
    if (d.attr&MSG_ATTR_PRESENT and not task){
        d.attr &= ~MSG_ATTR_PRESENT;
        result = ERROR_PORT_CLOSED;
    }

    if (d.attr & MSG_ATTR_PRESENT) {
        result = SUCCESS;

        klib::shared_ptr<Message> ptr = klib::make_shared<Message>(Message({pid_from, d.channel, klib::forward<klib::vector<char>>(msg)}));

        {
            Auto_Lock_Scope messaging_lock(task->messaging_lock);
            result = queue_message(task, ptr);
        }

        if (result == SUCCESS) unblock_if_needed(task, MESSAGE_S_NUM);
    } else if (d.attr & MSG_ATTR_DUMMY) {
        return d.enqueue(pid_from, klib::forward<klib::vector<char>>(msg));
    } if (not (d.attr & MSG_ATTR_NODEFAULT)) {
        result = ERROR_PORT_NOT_EXISTS;
    }

    return result;
}