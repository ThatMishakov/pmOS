#include "fs_consumer.hh"
#include <pmos/system.h>
#include "ipc.hh"

std::unordered_map<uint64_t, FsConsumer> fs_consumers;

void register_pipe_with_consumer(uint64_t pipe_port, uint64_t consumer_id)
{
    auto it = fs_consumers.find(consumer_id);

    if (it == fs_consumers.end()) {
        syscall_r r = set_task_group_notifier_mask(consumer_id, main_port, NOTIFICATION_MASK_DESTROYED);
        if (r.result != SUCCESS)
            throw std::system_error(-r.result, std::system_category());

        it = fs_consumers.emplace(consumer_id, FsConsumer{}).first;
    }

    auto &c = it->second;
    c.pipe_ports[consumer_id] = pipe_port;
}