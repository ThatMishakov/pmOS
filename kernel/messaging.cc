#include "messaging.hh"
#include "types.hh"
#include "sched.hh"
#include <stdint.h>
#include <stddef.h>
#include <kernel/errors.h>
#include "lib/utility.hh"
#include <kernel/block.h>

Ports_storage* kernel_ports;

kresult_t init_kernel_ports()
{
    kernel_ports = new Ports_storage;

    //kernel_ports->set_dummy(1); // Kernel log messages

    return SUCCESS;
}

kresult_t queue_message(TaskDescriptor* task, uint64_t from, uint64_t channel, char* message_usr_ptr, size_t size, bool from_user)
{
    Message msg;
    msg.from = from;
    msg.channel = channel;
    msg.content = klib::vector<char>(size);

    kresult_t copy_result;
    if (from_user)
        copy_result = copy_from_user(message_usr_ptr, &msg.content.front(), size);
    else {
        copy_result = SUCCESS;
        memcpy(message_usr_ptr, &msg.content.front(), size);
    }

    if (copy_result == SUCCESS) {
        task->messages.emplace(klib::move(msg));
    }

    return copy_result;
}

kresult_t Message::copy_to_user_buff(char* buff)
{
    return copy_to_user(&content.front(), buff, content.size());
}

kresult_t Port::enqueue(uint64_t from, uint64_t size, char* ptr, bool from_user)
{
    Message msg;
    msg.from = from;
    msg.channel = 0;
    msg.content = klib::vector<char>(size);

    kresult_t copy_result;
    if (from_user)
        copy_result = copy_from_user(ptr, &msg.content.front(), size);
    else {
        copy_result = SUCCESS;
        memcpy(ptr, &msg.content.front(), size);
    }

    if (copy_result == SUCCESS) {
        this->msg_queue.emplace(klib::move(msg));
    }

    return copy_result;
}

kresult_t Ports_storage::send_from_user(uint64_t pid_from, uint64_t port, uint64_t buff_addr, size_t size)
{
    if (this->storage.count(port) == 0) return ERROR_PORT_NOT_EXISTS;

    Port& d = this->storage.at(port);

    kresult_t result = SUCCESS;

    if (d.attr & 0x01) {
        d.enqueue(pid_from, size, (char*)buff_addr);
    } else if (not exists_process(d.task)){
        this->storage.erase(port);
        return ERROR_PORT_CLOSED;
    } else {
        TaskDescriptor* process = (*s_map).at(d.task);
        result = SUCCESS;

        process->lock.lock();
        result = queue_message(process, pid_from, d.channel, (char*)buff_addr, size);
        process->unblock_if_needed(MESSAGE_S_NUM);
        process->lock.unlock();
    }

    return result;
}

kresult_t Ports_storage::set_port(uint64_t port, uint64_t dest_pid, uint64_t dest_chan)
{
    if (not exists_process(dest_pid)) return ERROR_NO_SUCH_PROCESS;

    if (this->storage.count(port) == 1) {
        Port& p = this->storage.at(port);

        if (not p.msg_queue.empty()) {
            TaskDescriptor* d = get_task(dest_pid);

            while (not p.msg_queue.empty()) {
                d->messages.emplace(klib::move(p.msg_queue.front()));
                p.msg_queue.pop();
            }
        }
    }

    this->storage.insert({port, {dest_pid, dest_chan, 0, {}}});

    return SUCCESS;
}

kresult_t Ports_storage::set_dummy(uint64_t port)
{
    if (this->storage.count(port) == 1) {
        this->storage.at(port).attr |= 0x01;
    } else {
        this->storage.insert({port, {0,0,0x01, {}}});
    }
}