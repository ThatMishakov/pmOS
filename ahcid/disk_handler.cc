#include "disk_handler.hh"

#include "port.hh"

#include <cassert>
#include <cinttypes>
#include <errno.h>
#include <memory>
#include <pmos/memory.h>
#include <pmos/utility/scope_guard.hh>
#include <system_error>
#include <unordered_map>

extern pmos_port_t ahci_port;

using dptr = std::unique_ptr<DiskHandler>;
std::unordered_map<uint64_t, dptr> handlers;
uint64_t id = 1;
auto new_id() { return id++; }

using gptr = std::unique_ptr<TaskGroup>;
std::unordered_map<uint64_t, gptr> groups;

uint64_t align_to_page(uint64_t size) { return (size + 0xfff) & ~(uint64_t)0xfff; }

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

    auto [result, _] =
        set_task_group_notifier_mask(group_id, ahci_port, NOTIFICATION_MASK_DESTROYED, 0);
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

void handle_disk_read(pmos_port_t port, uint64_t memory_object, int result, uint64_t user_arg)
{
    // TODO: Memory object handling
    IPC_Disk_Read_Reply reply = {
        .type        = IPC_Disk_Read_Reply_NUM,
        .flags       = 0,
        .result_code = static_cast<int16_t>(result),
        .user_arg    = user_arg,
    };

    printf("Sending reply to disk read status %i\n", result);

    auto r = send_message_port2(port, memory_object, sizeof(reply), (void *)&reply, 0);
    if (r != 0)
        throw std::system_error(-r, std::system_category());
}

void handle_disk_read_error(pmos_port_t port, int result, uint64_t user_arg)
{
    try {
        handle_disk_read(port, 0, -result, user_arg);
    } catch (const std::exception &e) {
        printf("Exception in handle_disk_read_error: %s\n", e.what());
    }
}

pmos::async::detached_task handle_disk_read(const Message_Descriptor &d, IPC_Disk_Read request)
{
    if (!sender_is_of_group(request.disk_consumer_id, d)) {
        handle_disk_read_error(request.reply_port, -EPERM, request.user_arg);
        co_return;
    }

    auto disk_id = request.disk_id;
    DiskHandler *handler;
    try {
        handler = DiskHandler::get(disk_id);
    } catch (const std::out_of_range &) {
        handle_disk_read_error(request.reply_port, -ENOENT, request.user_arg);
        co_return;
    }

    auto it = handler->task_group_tree.find(request.disk_consumer_id);
    if (it == handler->task_group_tree.end()) {
        handle_disk_read_error(request.reply_port, -EPERM, request.user_arg);
        co_return;
    }

    auto sector_start = request.start_sector;
    auto sector_count = request.sector_count;

    if (sector_count == 0) {
        handle_disk_read_error(request.reply_port, -EINVAL, request.user_arg);
        co_return;
    }

    if (sector_start > handler->get_sector_count() || (sector_start > UINT64_MAX - sector_count) ||
        sector_start + sector_count > handler->get_sector_count()) {
        handle_disk_read_error(request.reply_port, -E2BIG, request.user_arg);
        co_return;
    }

    auto size         = sector_count * handler->get_logical_sector_size();
    auto size_aligned = align_to_page(size);

    auto res = create_mem_object(size_aligned, CREATE_FLAG_DMA | CREATE_FLAG_ALLOW_DISCONTINUOUS);
    if (res.result != SUCCESS) {
        handle_disk_read_error(request.reply_port, res.result, request.user_arg);
        co_return;
    }

    auto object = res.object_id;
    pmos::utility::scope_guard guard {[=] { release_mem_object(object, 0); }};

    try {
        // Casually try to read the disk
        uint64_t bytes_read = 0;

        auto [result, phys_addr] = get_page_phys_address_from_object(object, 0, 0);
        if (result != SUCCESS) {
            printf("Error reading page from object\n");
            handle_disk_read_error(request.reply_port, -result, request.user_arg);
            co_return;
        }

        auto sector_size = handler->get_logical_sector_size();

        auto page_size                = 4096; // TODO: don't hardcode this
        phys_addr_t current_phys_addr = phys_addr;
        uint64_t phys_addr_of_offset  = 0;
        while (bytes_read < size) {
            Command cmd = co_await Command::prepare(handler->get_port());

            uint64_t offset            = phys_addr_of_offset;
            uint64_t pushed_max_offset = offset;
            uint64_t start_offset      = offset;
            while (!cmd.prdt_full() and pushed_max_offset < size and
                   (offset - start_offset) / sector_size < 0xffff) {
                while (offset < size) {
                    if (offset == phys_addr_of_offset && offset + page_size >= size) {
                        cmd.prdt_push(
                            {.data_base       = current_phys_addr,
                             .rsv0            = 0,
                             .data_base_count = static_cast<uint32_t>(size - pushed_max_offset - 1),
                             .rsv1            = 0,
                             .interrupt_on_completion = 1});
                        pushed_max_offset = size;
                        break;
                    } else if (offset == phys_addr_of_offset) {
                        offset += page_size;
                    } else {
                        auto [result, phys_addr] = get_page_phys_address_from_object(object, offset, 0);
                        if (result != SUCCESS) {
                            handle_disk_read_error(request.reply_port, result, request.user_arg);
                            co_return;
                        }

                        bool too_much_for_prdt =
                            offset - phys_addr_of_offset + page_size > PRDT::MAX_BYTES;
                        // This part is questionable...
                        bool too_many_sectors =
                            (offset - start_offset) / handler->get_logical_sector_size() >= 0xffff;

                        if (current_phys_addr + offset - phys_addr_of_offset == phys_addr &&
                            !(too_much_for_prdt || too_many_sectors)) {
                            offset += page_size;
                            continue;
                        }

                        cmd.prdt_push({.data_base = current_phys_addr,
                                       .rsv0      = 0,
                                       .data_base_count =
                                           static_cast<uint32_t>(offset - phys_addr_of_offset - 1),
                                       .rsv1                    = 0,
                                       .interrupt_on_completion = 1});

                        current_phys_addr   = phys_addr;
                        phys_addr_of_offset = offset;
                        pushed_max_offset   = offset;
                        break;
                    }
                }
            }

            auto sectors_count = (pushed_max_offset - start_offset) / sector_size;
            auto start_sector  = start_offset / sector_size;

            cmd.sector       = start_sector;
            cmd.sector_count = sectors_count;

            auto result = co_await cmd.execute(0x25, 30'000);
            if (result != Command::Result::Success) {
                handle_disk_read_error(request.reply_port, -EIO, request.user_arg);
                co_return;
            }

            bytes_read = pushed_max_offset;
        }

        handle_disk_read(request.reply_port, object, 0, request.user_arg);
        guard.dismiss();
    } catch (const std::system_error &e) {
        printf("Error reading disk: %s\n", e.what());
        handle_disk_read_error(request.reply_port, -e.code().value(), request.user_arg);
    }
}

// This is horrible, why is it not in a header file?
extern std::vector<AHCIPort> ports;

AHCIPort &DiskHandler::get_port()
{
    return ports[port];
}