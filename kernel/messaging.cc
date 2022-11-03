#include "messaging.hh"
#include "types.hh"
#include "sched.hh"
#include <stdint.h>
#include <stddef.h>
#include "common/errors.h"
#include "lib/utility.hh"

kresult_t queue_message(TaskDescriptor* task, uint64_t from, char* message_usr_ptr, size_t size)
{
    Message msg;
    msg.from = from;
    msg.content = klib::vector<char>(size);

    kresult_t copy_result = copy_from_user(message_usr_ptr, &msg.content.front(), size);

    if (copy_result == SUCCESS) {
        task->lock.lock();
        task->messages.emplace(klib::move(msg));
        task->lock.unlock();
    }

    return copy_result;
}