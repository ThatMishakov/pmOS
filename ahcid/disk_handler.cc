#include "disk_handler.hh"

#include <cinttypes>
#include <memory>
#include <unordered_map>
#include <errno.h>
#include <system_error>
#include <cassert>

extern pmos_port_t ahci_port;

using dptr = std::unique_ptr<DiskHandler>;
std::unordered_map<uint64_t, dptr> handlers;
uint64_t id = 1;
auto new_id() { return id++; }

using gptr = std::unique_ptr<TaskGroup>;
std::unordered_map<uint64_t, gptr> groups;

DiskHandler *DiskHandler::create(int port, uint64_t sector_count, std::size_t logical_sector_size,
                                 std::size_t physical_sector_size)
{
    auto d                  = dptr(new DiskHandler);
    d->disk_id              = new_id();
    d->sector_count         = sector_count;
    d->logical_sector_size  = logical_sector_size;
    d->physical_sector_size = physical_sector_size;
    d->port                 = port;

    auto p = d.get();

    handlers.insert({d->disk_id, std::move(d)});
    return p;
}

void DiskHandler::destroy() noexcept { handlers.erase(disk_id); }

DiskHandler *DiskHandler::get(uint64_t id) { return handlers.at(id).get(); }

void handle_register_disk_reply(const Message_Descriptor &, const IPC_Disk_Register_Reply *reply)
{
    auto disk_id = reply->disk_id;
    try {
        auto disk = DiskHandler::get(disk_id);
        if (!disk->h) {
            printf("DiskHandler not found\n");
            return;
        }

        disk->error_code           = reply->result_code;
        disk->diskd_event_port     = reply->disk_port;
        disk->blockd_task_group_id = reply->task_group_id;

        disk->h.resume();
    } catch (std::exception &e) {
        printf("Exception in handle_register_disk_reply: %s\n", e.what());
    }
}

void handle_disk_open_reply(int16_t result_code, uint64_t disk_id, uint16_t flags,
                            uint64_t disk_consumer_id, pmos_port_t reply_port)
{
    IPC_Disk_Open_Reply reply = {
        .type             = IPC_Disk_Open_Reply_NUM,
        .flags            = flags,
        .result_code      = result_code,
        .disk_id          = disk_id,
        .disk_port        = ahci_port,
        .task_group_id    = pmos_process_task_group(),
        .disk_consumer_id = disk_consumer_id,
    };

    auto result = send_message_port(reply_port, sizeof(reply), (void *)&reply);
    if (result != 0) {
        printf("[AHCId] Warning: Could not reply to IPC_Disk_Open, port %" PRIi64
               " error %i (%s)\n",
               reply_port, (int)-result, strerror(-result));
    }
}

void handle_disk_open(const Message_Descriptor &d, const IPC_Disk_Open *request)
{
    auto disk_id    = request->disk_id;
    auto task_group = request->disk_consumer_id;
    auto reply_port = request->reply_port;
    bool closing    = request->flags & IPC_DISK_OPEN_FLAG_CLOSE;
    auto flags      = closing ? IPC_DISK_OPEN_FLAG_CLOSE : 0;

    DiskHandler *handler = nullptr;
    try {
        handler = DiskHandler::get(disk_id);
    } catch (const std::out_of_range &) {
        handle_disk_open_reply(-ENOENT, disk_id, flags, task_group, reply_port);
        return;
    }

    if (!sender_is_of_group(handler->blockd_task_group_id, d)) {
        handle_disk_open_reply(-EPERM, disk_id, flags, task_group, reply_port);

        return;
    }

    auto it = handler->task_group_tree.find(task_group);
    if (it != handler->task_group_tree.end()) {
        auto group = it->group;
        if (!closing) {
            ++it->open_count;
            handle_disk_open_reply(0, disk_id, flags, task_group, reply_port);

            return;
        } else {
            auto t = --it->open_count;
            handle_disk_open_reply(0, disk_id, flags, task_group, reply_port);

            if (t == 0) {
                group->disk_tree.erase(it);
                handler->task_group_tree.erase(it);
                delete &*it;

                group->handle_maybe_empty();
            }
        }
    } else if (closing) {
        handle_disk_open_reply(-ENOENT, disk_id, flags, task_group, reply_port);

    } else {
        try {
            auto group    = TaskGroup::get_or_create(task_group);
            auto t        = new TaskGroupOpenDisk();
            t->disk       = handler;
            t->group      = group;
            t->open_count = 1;

            group->disk_tree.insert(t);
            handler->task_group_tree.insert(t);

            handle_disk_open_reply(0, disk_id, flags, task_group, reply_port);

        } catch (const std::system_error &e) {
            handle_disk_open_reply(-e.code().value(), disk_id, flags, task_group, reply_port);

            return;
        }
    }
}

bool CompareByTaskGroup::operator()(const TaskGroupOpenDisk &a, const TaskGroupOpenDisk &b)
{
    return a.group->task_group_id < b.group->task_group_id;
}

bool CompareByTaskGroup::operator()(uint64_t a, const TaskGroupOpenDisk &b)
{
    return a < b.group->task_group_id;
}

bool CompareByTaskGroup::operator()(const TaskGroupOpenDisk &a, uint64_t b)
{
    return a.group->task_group_id < b;
}

bool CompareByDisk::operator()(const TaskGroupOpenDisk &a, const TaskGroupOpenDisk &b)
{
    return a.disk->get_disk_id() < b.disk->get_disk_id();
}

bool sender_is_of_group(uint64_t task_group_id, Message_Descriptor msg)
{
    auto [result, code] = is_task_group_member(msg.sender, task_group_id);
    return (result == 0) && code;
}

TaskGroup *TaskGroup::get_or_create(uint64_t group_id)
{
    auto it = groups.find(group_id);
    if (it != groups.end()) {
        return it->second.get();
    }

    auto itn = groups.insert({group_id, std::make_unique<TaskGroup>(group_id)});
    assert(itn.second);

    auto [result, _] = set_task_group_notifier_mask(group_id, ahci_port, NOTIFICATION_MASK_DESTROYED, 0);
    if (result != 0) {
        groups.erase(itn.first);
        throw std::system_error(-result, std::system_category());
    }
    return itn.first->second.get();
}

void TaskGroup::handle_maybe_empty()
{
    if (!disk_tree.empty())
        return;

    remove();
}

void TaskGroup::remove()
{
    set_task_group_notifier_mask(task_group_id, ahci_port, 0, 0);
    groups.erase(task_group_id);
}