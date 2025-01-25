#include "partitions.hh"

#include <cinttypes>
#include <cstdio>
#include <map>
#include <pmos/async/coroutines.hh>
#include <pmos/containers/intrusive_bst.hh>
#include <pmos/containers/intrusive_list.hh>
#include <pmos/ipc.h>
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <pmos/utility/scope_guard.hh>
#include <stdio.h>
#include <string_view>
#include <sys/mman.h>
#include <system_error>
#include <unistd.h>
#include <vector>

size_t alignup(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

void ipc_send(const auto &data, pmos_port_t dest)
{
    auto size   = sizeof(data);
    auto result = send_message_port(dest, size, reinterpret_cast<const char *>(&data));
    if (result != 0)
        throw std::system_error(-result, std::system_category(), strerror(-result));
}

bool sender_is_of_group(uint64_t task_group_id, Message_Descriptor msg)
{
    auto [code, result] = is_task_group_member(msg.sender, task_group_id);
    return code == 0 && result == 1;
}

pmos_port_t create_main_port(std::string_view port_name)
{
    ports_request_t request = create_port(TASK_ID_SELF, 0);
    if (request.result != SUCCESS)
        throw std::system_error(-request.result, std::system_category());

    auto port = request.port;

    result_t r = name_port(port, port_name.data(), port_name.size(), 0);
    if (r != SUCCESS)
        throw std::system_error(-r, std::system_category());

    return port;
}

struct Disk;
struct DiskOpenAwaiter {
    Disk *disk;
    pmos::containers::RBTreeNode<DiskOpenAwaiter> node;
    uint64_t task_group_id;

    std::coroutine_handle<> h;
    const IPC_Disk_Open_Reply *reply;

    bool opened = false;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h_);
    const IPC_Disk_Open_Reply *await_resume() noexcept { return reply; }
};

using mem_object_id = uint64_t;
struct ReadAwaiter {
    std::coroutine_handle<> h {};
    mem_object_id mem_object {};

    Disk &disk;
    uint64_t sector_start;
    uint64_t sector_count;

    uint64_t op_id;

    pmos::containers::DoubleListHead<ReadAwaiter> list_node;
    pmos::containers::RBTreeNode<ReadAwaiter> node;

    ReadAwaiter(Disk &disk, uint64_t sector_start, uint64_t sector_count)
        : disk(disk), sector_start(sector_start), sector_count(sector_count)
    {
    }

    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h_);
    uint64_t await_resume() { return mem_object; }
};

pmos::containers::RedBlackTree<
    ReadAwaiter, &ReadAwaiter::node,
    detail::TreeCmp<ReadAwaiter, uint64_t, &ReadAwaiter::op_id>>::RBTreeHead read_awaiters;

DiskOpenAwaiter open_disk(Disk &disk, uint64_t task_group_id);

pmos_port_t main_port = create_main_port("/pmos/blockd");

struct Disk {
    size_t id;
    bool used;

    uint64_t driver_process_group;
    uint64_t driver_id;

    pmos_port_t driver_port;

    int logical_sector_size;
    int physical_sector_size;
    uint64_t sector_count;

    using tree = pmos::containers::RedBlackTree<
        DiskOpenAwaiter, &DiskOpenAwaiter::node,
        detail::TreeCmp<DiskOpenAwaiter, uint64_t, &DiskOpenAwaiter::task_group_id>>;

    tree::RBTreeHead open_awaiters;
    pmos::containers::InitializedCircularDoubleList<ReadAwaiter, &ReadAwaiter::list_node>
        read_awaiters;

    struct Partition {
        uint64_t start_lba;
        uint64_t end_lba;
    };

    std::vector<Partition> partitions;
};

std::vector<Disk> disks;

size_t prepare_unused_disk_index()
{
    for (size_t i = 0; i < disks.size(); ++i) {
        if (not disks[i].used)
            return i;
    }

    disks.push_back({});
    return disks.size() - 1;
}

std::map<std::pair<uint64_t, uint64_t>, size_t> index_by_port;

void register_disk_error(int code, uint64_t disk_id, pmos_port_t reply_port)
{
    IPC_Disk_Register_Reply reply = {
        .type          = IPC_Disk_Register_Reply_NUM,
        .flags         = 0,
        .result_code   = static_cast<int16_t>(-code),
        .disk_id       = disk_id,
        .disk_port     = 0,
        .task_group_id = 0,
    };

    try {
        ipc_send(reply, reply_port);
    } catch (std::system_error &e) {
        printf("[blockd] Warning: Registering disk resulted in %i. Failed to send reply with %s\n",
               code, e.what());
    }
}

DiskOpenAwaiter open_disk(Disk &disk, uint64_t task_group_id)
{
    DiskOpenAwaiter awaiter;
    awaiter.task_group_id  = task_group_id;
    IPC_Disk_Open open_msg = {
        .type             = IPC_Disk_Open_NUM,
        .flags            = 0,
        .reply_port       = main_port,
        .disk_id          = disk.driver_id,
        .task_group_id    = disk.driver_process_group,
        .disk_consumer_id = task_group_id,
    };

    ipc_send(open_msg, disk.driver_port);

    awaiter.reply = nullptr;
    awaiter.disk  = &disk;
    return awaiter;
}

uint64_t read_index = 0;
void ReadAwaiter::await_suspend(std::coroutine_handle<> h_)
{
    h = h_;

    auto idx = read_index++;

    IPC_Disk_Read read = {
        .type             = IPC_Disk_Read_NUM,
        .flags            = 0,
        .disk_id          = disk.driver_id,
        .task_group_id    = disk.driver_process_group,
        .disk_consumer_id = pmos_process_task_group(),
        .start_sector     = sector_start,
        .sector_count     = sector_count,
        .reply_port       = main_port,
        .user_arg         = idx,
    };

    ipc_send(read, disk.driver_port);

    op_id = idx;

    read_awaiters.insert(this);
    disk.read_awaiters.push_back(this);
}

void disk_open_msg(const char *tt, Message_Descriptor desc)
{
    auto reply         = reinterpret_cast<const IPC_Disk_Open_Reply *>(tt);
    auto task_group_id = reply->task_group_id;
    auto disk_id       = reply->disk_id;

    auto it = index_by_port.find({task_group_id, disk_id});
    if (it == index_by_port.end()) {
        printf("[blockd] Disk open reply for unknown disk %" PRIu64 " %" PRIu64 "\n", task_group_id,
               disk_id);
        return;
    }

    auto &disk = disks[it->second];

    // TODO: Verify that the sender is the driver
    (void)desc;

    auto open_task_group = reply->disk_consumer_id;
    Disk::tree::RBTreeIterator t;
    while ((t = disk.open_awaiters.find(open_task_group)) != disk.open_awaiters.end()) {
        t->reply  = reply;
        t->opened = true;
        disk.open_awaiters.erase(t);
        t->h.resume();
    }
}

void disk_read_msg(const char *tt, Message_Descriptor desc)
{
    auto reply    = reinterpret_cast<const IPC_Disk_Read_Reply *>(tt);
    auto user_arg = reply->user_arg;

    auto it = read_awaiters.find(user_arg);
    if (it == read_awaiters.end()) {
        printf("[blockd] Disk read reply for unknown operation %" PRIu64 "\n", user_arg);
        return;
    }

    auto &await = *it;
    read_awaiters.erase(it);
    await.disk.read_awaiters.remove(&await);

    await.mem_object = desc.mem_object;
    await.h.resume();
}

ReadAwaiter read_disk(Disk &disk, uint64_t from_sector, uint64_t sector_count)
{
    return ReadAwaiter(disk, from_sector, sector_count);
}

void DiskOpenAwaiter::await_suspend(std::coroutine_handle<> h_)
{
    h = h_;
    disk->open_awaiters.insert(this);
}

size_t align_to_page(size_t size)
{
    auto page_size = 4096; // TODO: don't hardcode this
    return (size + page_size - 1) & ~(page_size - 1);
}

pmos::async::detached_task probe_partitions(size_t disk_idx)
{
    auto &disk  = disks[disk_idx];
    // Read partition table
    auto object = co_await read_disk(disk, 0, 2);
    printf("Mem object: %lu\n", object);

    if (object == 0) {
        printf("Failed to read partition table off disk %" PRIi64 " %" PRIi64 "\n", disk.driver_id,
               disk.driver_process_group);
        co_return;
    }

    pmos::utility::scope_guard guard {[&] { release_mem_object(object, 0); }};
    auto mbr_size          = align_to_page(disk.logical_sector_size * 2);
    auto [result, mbr_ptr] = map_mem_object(0, nullptr, mbr_size, PROT_READ, object, 0);
    if (result != SUCCESS) {
        printf("Failed to map partition table %i (%s)\n", -(int)result, strerror(-result));
        co_return;
    }
    pmos::utility::scope_guard guard2 {[&] { munmap(mbr_ptr, mbr_size); }};

    auto *mbr = reinterpret_cast<MBR *>(mbr_ptr);
    if (mbr->magic != mbr->MAGIC) {
        printf("Invalid MBR magic\n");
        co_return;
    }

    if (mbr->partitions[0].type == 0) {
        printf("No partitions\n");
        co_return;
    } else if (mbr->partitions[0].type == 0xee) {
        printf("MBR protective partition found...\n");
        auto gpt = reinterpret_cast<GPTHeader *>(reinterpret_cast<char *>(mbr) + disk.logical_sector_size);
        if (std::memcmp(gpt->signature, "EFI PART", 8) != 0) {
            printf("Invalid GPT signature\n");
            co_return;
        }

        if (!verify_gpt_checksum(*gpt)) {
            printf("Invalid GPT checksum\n");
            co_return;
        }

        auto gpt_first_lba = gpt->first_usable_lba;
        if (gpt_first_lba < 2) {
            printf("Invalid GPT first LBA\n");
            co_return;
        }

        printf("Partition entry LBA: %" PRIu64 "\n", gpt->partition_entry_lba);

        auto gpt_entry_size = gpt->partition_entry_size;
        auto gpt_entry_count = gpt->num_partition_entries;

        printf("GPT partition table: %i entries, %i bytes each\n", gpt_entry_count, gpt_entry_size);

        auto gpt_partition_array_size = gpt_entry_size * gpt_entry_count;
        if (gpt_partition_array_size > 0x100000) {
            printf("GPT partition array too large\n");
            co_return;
        }

        auto number_of_sectors = alignup(gpt_partition_array_size, disk.logical_sector_size) /
                                 disk.logical_sector_size;
        auto gpt_object = co_await read_disk(disk, gpt->partition_entry_lba, number_of_sectors);
        if (gpt_object == 0) {
            printf("Failed to read GPT partition array\n");
            co_return;
        }
        pmos::utility::scope_guard guard3 {[&] { release_mem_object(gpt_object, 0); }};
        auto array_size_aligned = align_to_page(gpt_partition_array_size);
        auto [result, gpt_ptr] = map_mem_object(0, nullptr, array_size_aligned, PROT_READ,
                                                gpt_object, 0);
        if (result != SUCCESS) {
            printf("Failed to map GPT partition array %i (%s)\n", -(int)result, strerror(-result));
            co_return;
        }
        pmos::utility::scope_guard guard4 {[&] { munmap(gpt_ptr, array_size_aligned); }};


        std::vector<Disk::Partition> partitions;
        for (size_t offset = 0; offset < gpt_partition_array_size; offset += gpt_entry_size) {
            auto *entry = reinterpret_cast<GPTPartitionEntry *>(reinterpret_cast<char *>(gpt_ptr) + offset);
            if (guid_zero(entry->type_guid))
                continue;

            std::string guid = guid_to_string(entry->type_guid);
            printf("GPT partition: type %s, start %" PRIu64 ", end %" PRIu64 " offset %" PRIu64 "\n", guid.c_str(),
                   entry->first_lba, entry->last_lba, offset/gpt_entry_size);
            partitions.push_back({entry->first_lba, entry->last_lba});
        }
        disk.partitions = std::move(partitions);
    } else {
        printf("MBR partition table:\n");
        std::vector<Disk::Partition> partitions;
        for (int i = 0; i < 4; ++i) {
            auto &part = mbr->partitions[i];
            if (part.type == 0)
                continue;

            printf("Partition %i: type %x, start %" PRIu32 ", size %" PRIu32 "\n", i, part.type,
                   part.lba_start, part.num_sectors);

            partitions.push_back({part.lba_start, part.lba_start + part.num_sectors});
        }
        disk.partitions = std::move(partitions);
    }
}

pmos::async::detached_task register_disk(char *i, Message_Descriptor d)
{
    auto msg        = reinterpret_cast<IPC_Disk_Register *>(i);
    auto group_id   = msg->task_group_id;
    auto disk_id    = msg->disk_id;
    auto reply_port = msg->reply_port;

    if (!sender_is_of_group(group_id, d)) {
        register_disk_error(EPERM, disk_id, reply_port);
        co_return;
    }

    if (index_by_port.count({group_id, disk_id})) {
        register_disk_error(EEXIST, disk_id, reply_port);
        co_return;
    }

    try {
        auto index = prepare_unused_disk_index();

        auto &disk = disks[index];

        pmos::utility::scope_guard guard {[&] { disk.used = false; }};

        index_by_port.insert({{group_id, disk_id}, index});

        disk.used                 = true;
        disk.id                   = index;
        disk.driver_process_group = group_id;
        disk.driver_id            = disk_id;
        disk.driver_port          = msg->disk_port;
        disk.logical_sector_size  = msg->logical_sector_size;
        disk.physical_sector_size = msg->physical_sector_size;
        disk.sector_count         = msg->sector_count;

        IPC_Disk_Register_Reply reply = {
            .type          = IPC_Disk_Register_Reply_NUM,
            .flags         = 0,
            .result_code   = 0,
            .disk_id       = disk_id,
            .disk_port     = main_port,
            .task_group_id = pmos_process_task_group(),
        };

        ipc_send(reply, msg->reply_port);

        guard.dismiss();

        auto open_reply = co_await open_disk(disk, pmos_process_task_group());

        if (open_reply->result_code != 0) {
            printf("[blockd] Disk %" PRIu64 " open failed: %i\n", disk_id, open_reply->result_code);
            co_return;
        }
        printf("[blockd] Disk %" PRIu64 " opened\n", disk_id);

        probe_partitions(index);

        co_return;
    } catch (std::system_error &e) {
        register_disk_error(e.code().value(), disk_id, reply_port);
        co_return;
    }
}

int main()
{
    while (true) {
        Message_Descriptor msg;
        syscall_get_message_info(&msg, main_port, 0);

        auto msg_buff = std::make_unique<char[]>(msg.size);

        get_first_message(msg_buff.get(), 0, main_port);

        if (msg.size < sizeof(IPC_Generic_Msg)) {
            fprintf(stderr, "Warning: recieved very small message\n");
            break;
        }

        IPC_Generic_Msg *ipc_msg = reinterpret_cast<IPC_Generic_Msg *>(msg_buff.get());
        switch (ipc_msg->type) {
        case IPC_Disk_Register_NUM:
            register_disk(msg_buff.get(), msg);
            break;
        case IPC_Disk_Open_Reply_NUM:
            disk_open_msg(msg_buff.get(), msg);
            break;
        case IPC_Disk_Read_Reply_NUM:
            disk_read_msg(msg_buff.get(), msg);
            break;
        default:
            printf("Received message of type %d\n", ipc_msg->type);
        }
    }
}