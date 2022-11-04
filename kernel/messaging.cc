#include "messaging.hh"
#include "types.hh"
#include "sched.hh"
#include <stdint.h>
#include <stddef.h>
#include <kernel/errors.h>
#include "lib/utility.hh"

kresult_t queue_message(TaskDescriptor* task, uint64_t from, uint64_t channel, char* message_usr_ptr, size_t size)
{
    Message msg;
    msg.from = from;
    msg.channel = channel;
    msg.content = klib::vector<char>(size);

    kresult_t copy_result = copy_from_user(message_usr_ptr, &msg.content.front(), size);

    if (copy_result == SUCCESS) {
        task->messages.emplace(klib::move(msg));
    }

    return copy_result;
}

kresult_t Message::copy_to_user_buff(char* buff)
{
    return copy_to_user(&content.front(), buff, content.size());
}