#include "fs_consumer.hh"
#include "ipc.hh"
#include <pmos/system.h>
#include <system_error>

std::unordered_map<uint64_t, FsConsumer> fs_consumers;

void register_pipe_with_consumer(uint64_t pipe_port, uint64_t consumer_id)
{
    auto it = fs_consumers.find(consumer_id);

    if (it == fs_consumers.end()) {
        syscall_r r =
            set_task_group_notifier_mask(consumer_id, main_port, NOTIFICATION_MASK_DESTROYED, 0);
        if (r.result != SUCCESS)
            throw std::system_error(-r.result, std::system_category());

        it = fs_consumers.emplace(consumer_id, FsConsumer {}).first;
    }

    auto &c = it->second;
    c.pipe_ports.insert(pipe_port);
}

void unregister_pipe_with_consumer(uint64_t pipe_port, uint64_t consumer_id)
{
    auto it = fs_consumers.find(consumer_id);
    if (it == fs_consumers.end())
        return;

    auto &c = it->second;
    c.pipe_ports.erase(pipe_port);

    if (c.pipe_ports.empty()) {
        set_task_group_notifier_mask(consumer_id, main_port, 0, 0);

        fs_consumers.erase(it);
    }
}

void notify_consumer_destroyed(pmos_port_t pipe_port, uint64_t consumer_id)
{
    try {
        IPCNotifyConsumerDestroyed msg = {
            .consumer_id = consumer_id,
        };

        send_message(pipe_port, msg);
    } catch (...) {
    }
}

void handle_consumer_destroyed(uint64_t consumer_id)
{
    auto it = fs_consumers.find(consumer_id);
    if (it == fs_consumers.end())
        return;

    auto &c = it->second;
    for (auto p: c.pipe_ports)
        notify_consumer_destroyed(p, consumer_id);
}
