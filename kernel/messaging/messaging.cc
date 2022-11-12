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
    kernel_ports.set_dummy(1); // General log messages

    return SUCCESS;
}

kresult_t queue_message(TaskDescriptor* task, u64 from, u64 channel, klib::vector<char>&& msg_v)
{
    Message msg;
    msg.from = from;
    msg.channel = channel;
    msg.content = klib::forward<klib::vector<char>>(msg_v);

    task->messages.emplace(klib::move(msg));

    return SUCCESS;
}

kresult_t Message::copy_to_user_buff(char* buff)
{
    return copy_to_user(&content.front(), buff, content.size());
}

kresult_t Port::enqueue(u64 from, klib::vector<char>&& msg_v)
{
    Message msg;
    msg.from = from;
    msg.channel = 0;
    msg.content = klib::forward<klib::vector<char>>(msg_v);

    this->msg_queue.emplace(klib::move(msg));

    return SUCCESS;
}

kresult_t Ports_storage::send_from_user(u64 pid_from, u64 port, u64 buff_addr, size_t size)
{
    if (this->storage.count(port) == 0) return ERROR_PORT_NOT_EXISTS;
    klib::vector<char> message(size);

    kresult_t result = copy_from_user((char*)buff_addr, &message.front(), size);

    if (result != SUCCESS) return result;

    Port& d = this->storage.at(port);

    if (d.attr & 0x01) {
        d.enqueue(pid_from, move(message));
    } else if (not exists_process(d.task)){
        this->storage.erase(port);
        return ERROR_PORT_CLOSED;
    } else {
        TaskDescriptor* process = s_map.at(d.task);
        result = SUCCESS;

        process->lock.lock();
        result = queue_message(process, pid_from, d.channel, move(message));
        process->unblock_if_needed(MESSAGE_S_NUM);
        process->lock.unlock();
    }

    return result;
}

kresult_t Ports_storage::send_from_system(u64 port, const char* msg, size_t size)
{
    static const u64 pid_from = 0;
    if (this->storage.count(port) == 0) return ERROR_PORT_NOT_EXISTS;
    klib::vector<char> message(size);
    memcpy(msg, &message.front(), size);

    Port& d = this->storage.at(port);

    kresult_t result = SUCCESS;

    if (d.attr & 0x01) {
        d.enqueue(pid_from, klib::move(message));
    } else if (not exists_process(d.task)){
        this->storage.erase(port);
        return ERROR_PORT_CLOSED;
    } else {
        TaskDescriptor* process = s_map.at(d.task);
        result = SUCCESS;

        process->lock.lock();
        result = queue_message(process, pid_from, d.channel, klib::move(message));
        if (result == SUCCESS) process->unblock_if_needed(MESSAGE_S_NUM);
        process->lock.unlock();
    }

    return result;
}

kresult_t Ports_storage::set_port(u64 port, u64 dest_pid, u64 dest_chan)
{
    if (not exists_process(dest_pid)) return ERROR_NO_SUCH_PROCESS;

    if (this->storage.count(port) == 1) {
        Port& p = this->storage.at(port);

        p.task = dest_pid;
        p.channel = dest_chan;
        p.attr &= ~0x01ULL;

        if (not p.msg_queue.empty()) {
            TaskDescriptor* d = get_task(dest_pid);

            while (not p.msg_queue.empty()) {
                Message& m = p.msg_queue.front();
                m.channel = dest_chan;
                d->messages.push(klib::move(m));
                p.msg_queue.pop();
            }

            d->unblock_if_needed(MESSAGE_S_NUM);
        }
    }

    this->storage.insert({port, {dest_pid, dest_chan, 0, {}}});

    return SUCCESS;
}

kresult_t Ports_storage::set_dummy(u64 port)
{
    if (this->storage.count(port) == 1) {
        this->storage.at(port).attr |= 0x03;
    } else {
        this->storage.insert({port, {0,0,0x03, {}}});
    }

    return SUCCESS;
}

kresult_t send_message_system(u64 port, const char* msg, size_t size)
{
    return kernel_ports.send_from_system(port, msg, size);
}