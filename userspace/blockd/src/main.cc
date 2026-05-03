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
#include <limits.h>
#include <set>
#include <algorithm>
#include <pmos/helpers.hh>
#include <pmos/pmbus_helper.hh>
#include <pmos/ipc/bus_object.hh>
#include <pmos/fs_properties.hh>

using namespace pmos;
using namespace pmos::ipc;

pmos::Port port = pmos::Port::create().value();
pmos::PortDispatcher dispatcher(port);
pmos::PMBUSHelper bus_helper(dispatcher);

size_t alignup(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
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

struct Filesystem;

using FSProperties = std::vector<FSProperty>;

struct Partition {
    uint64_t start_lba;
    uint64_t end_lba;
    pmos::Right partition_right;

    std::map<uint64_t, FSProperties> infos;

    Partition(uint64_t start_lba, uint64_t end_lba): start_lba(start_lba), end_lba(end_lba), partition_right({}), infos({}) {}
};

struct Disk {
    size_t id;
    bool used;

    int logical_sector_size;
    int physical_sector_size;
    uint64_t sector_count;

    std::vector<std::shared_ptr<Partition>> partitions;

    pmos::Right disk_right;

    std::string name;
};
std::set<std::shared_ptr<Partition>> ready_partitions;

std::vector<Disk> disks;

uint64_t filesystem_next_id = 1;

struct Filesystem {
    pmos::Right service_right;
    std::set<std::shared_ptr<Partition>> probed_partitions;
    uint64_t id = filesystem_next_id++;
    std::string name;
};
std::map<uint64_t, std::shared_ptr<Filesystem>> filesystems;

struct MountpointRequest {
    std::string mount_path;
    std::string label;
    // TODO I guess
};

std::vector<MountpointRequest> mountpoint_requests = {
    {.mount_path = "/", .label = "pmos-root"},
};

pmos::async::detached_task mount_partition(std::shared_ptr<Filesystem> fs, std::shared_ptr<Partition> partition, const std::string &mount_path)
{
    IPC_Start_Service req = {
        .type = IPC_Start_Service_NUM,
        .flags = 0,
    };

    std::vector<std::byte> data;
    auto span = std::span<const std::byte>(reinterpret_cast<const std::byte *>(&req), sizeof(req));
    data.insert(data.end(), span.begin(), span.end());

    std::string cmdline = "--mount --disk $RIGHT1 --reply $RIGHT0 --mountpoint " + mount_path;
    auto cmd_span = std::as_bytes(std::span(cmdline.data(), cmdline.size()));
    data.insert(data.end(), cmd_span.begin(), cmd_span.end());

    auto partition_right = partition->partition_right.clone_noexcept();
    if (!partition_right) {
        printf("Failed to clone the partition right...\n");
        co_return;
    }
    auto reply_right = port.create_right(RightType::SendOnce).value();
    auto result = pmos::send_message_right(fs->service_right, std::span<const std::byte>(data), {}, false, std::move(reply_right.first), std::move(partition_right.value()));
    if (!result) {
        printf("Failed to send the message to start the service...\n");
        co_return;
    }

    auto msg = co_await dispatcher.get_message(reply_right.second);

    if (!msg) {
        printf("Failed to get the reply message...\n");
        co_return;
    }

    if (msg->descriptor.size < sizeof(IPC_FS_Mount_Request_Result)) {
        printf("Invalid reply size for FS mount result\n");
        co_return;
    }

    auto *reply = reinterpret_cast<IPC_FS_Mount_Request_Result *>(msg->data.data());
    if (reply->type != IPC_FS_Mount_Request_Result_NUM) {
        printf("Invalid reply type for FS mount result (%i)\n", reply->type);
        co_return;
    }

    if (reply->result_code == 0) {
        printf("Successfully mounted filesystem %s at mount point %s\n", fs->name.c_str(), mount_path.c_str());
    } else {
        printf("Failed to mount filesystem %s at mount point %s\n", fs->name.c_str(), mount_path.c_str());
    }
}

void try_mount_new_fs(std::shared_ptr<Filesystem> fs, std::shared_ptr<Partition> partition)
{
    auto properties = partition->infos.at(fs->id);

    for (const auto &req: mountpoint_requests) {
        if (std::ranges::any_of(properties, [&](const FSProperty &prop) {
                if (auto *label = std::get_if<FSPropertyLabel>(&prop))
                    return label->label == req.label;
                return false;
            })) {
            printf("Mounting filesystem %s on partition with label %s at mount point %s\n", fs->name.c_str(), req.label.c_str(), req.mount_path.c_str());

            mount_partition(fs, partition, req.mount_path);
        }
    }
}


size_t prepare_unused_disk_index()
{
    for (size_t i = 0; i < disks.size(); ++i) {
        if (not disks[i].used)
            return i;
    }

    disks.push_back({});
    return disks.size() - 1;
}

size_t align_to_page(size_t size)
{
    auto page_size = PAGE_SIZE; // TODO: don't hardcode this
    return (size + page_size - 1) & ~(page_size - 1);
}

pmos::async::task<pmos::Right> read_disk(Disk &disk, uint64_t sector_start, uint64_t sector_count)
{
    IPC_Disk_Read read = {
        .type             = IPC_Disk_Read_NUM,
        .flags            = 0,
        .start_sector     = sector_start,
        .sector_count     = sector_count,
    };

    auto span = std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(&read), sizeof(read));
    auto result = send_message_right(disk.disk_right, span, std::pair{&dispatcher.get_port(), RightType::SendOnce}, false);
    if (!result)
        throw std::system_error(result.error(), std::system_category());

    auto msg = co_await dispatcher.get_message(result.value());
    if (!msg)
        throw std::system_error(msg.error(), std::system_category());

    if (msg->descriptor.size < sizeof(IPC_Disk_Read_Reply))
        throw std::system_error(EINTR, std::system_category());

    auto *reply = reinterpret_cast<IPC_Disk_Read_Reply *>(msg->data.data());
    if (reply->type != IPC_Disk_Read_Reply_NUM)
        throw std::system_error(EINTR, std::system_category());

    if (reply->result_code != 0)
        throw std::system_error(-reply->result_code, std::system_category());

    if (!msg->other_rights[0] || msg->other_rights[0].type() != RightType::MemObject)
        throw std::system_error(EINTR, std::system_category());

    co_return std::move(msg->other_rights[0]);
}

pmos::async::task<std::optional<pmos::Right>> get_partition_right(Disk &disk, const Partition &partition)
{
    try {
        IPC_Disk_Create_Right r = {
            .type = IPC_Disk_Create_Right_NUM,
            .flags = 0,
            .start_sector = partition.start_lba,
            .sector_count = partition.end_lba - partition.start_lba + 1,
        };

        auto result = send_message_right_one(disk.disk_right, r, std::pair{&dispatcher.get_port(), RightType::SendOnce}, false);
        if (!result)
            co_return std::nullopt;

        auto msg = co_await dispatcher.get_message(result.value());

        if (msg.value().descriptor.size < sizeof(IPC_Disk_Create_Right_Reply))
            co_return std::nullopt;

        auto *reply = reinterpret_cast<IPC_Disk_Create_Right_Reply *>(msg->data.data());
        if (reply->type != IPC_Disk_Create_Right_Reply_NUM)
            co_return std::nullopt;

        if (reply->result_code != 0)
            co_return std::nullopt;

        if (!msg->other_rights[0] || msg->other_rights[0].type() != RightType::SendMany)
            co_return std::nullopt;

        co_return std::move(msg->other_rights[0]);
    } catch (...) {
        co_return std::nullopt;
    }
}

pmos::async::task<void> create_partition_rights(size_t disk_idx)
{
    auto &disk  = disks[disk_idx];

    for (auto &partition: disk.partitions) {
        auto right = co_await get_partition_right(disk, *partition);
        if (right) {
            partition->partition_right = std::move(right.value());
            printf("Got right for a partition...\n");
        } else {
            printf("Failed to get right for a partition!\n");
        }
    }
}

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

pmos::async::detached_task probe_partition(std::shared_ptr<Filesystem> fs, std::shared_ptr<Partition> partition)
{
    printf("Probing partition for filesystem %s\n", fs->name.c_str());

    IPC_Start_Service req = {
        .type = IPC_Start_Service_NUM,
        .flags = 0,
    };

    std::vector<std::byte> data;
    auto span = std::span<const std::byte>(reinterpret_cast<const std::byte *>(&req), sizeof(req));
    data.insert(data.end(), span.begin(), span.end());

    std::string_view cmdline = "--probe --disk $RIGHT1 --reply $RIGHT0";
    auto cmd_span = std::as_bytes(std::span(cmdline.data(), cmdline.size()));
    data.insert(data.end(), cmd_span.begin(), cmd_span.end());

    auto partition_right = partition->partition_right.clone_noexcept();
    if (!partition_right) {
        printf("Failed to clone the partition right...\n");
        co_return;
    }
    auto reply_right = port.create_right(RightType::SendOnce).value();
    auto result = pmos::send_message_right(fs->service_right, std::span<const std::byte>(data), {}, false, std::move(reply_right.first), std::move(partition_right.value()));
    if (!result) {
        printf("Failed to send the message to start the service...\n");
        co_return;
    }

    auto msg = co_await dispatcher.get_message(reply_right.second);

    if (!msg) {
        printf("Failed to get the reply message...\n");
        co_return;
    }

    if (msg->descriptor.size < sizeof(IPC_FS_Probe_Result)) {
        printf("Invalid reply size for FS probe result\n");
        co_return;
    }

    auto *reply = reinterpret_cast<IPC_FS_Probe_Result *>(msg->data.data());
    if (reply->type != IPC_FS_Probe_Result_NUM) {
        printf("Invalid reply type for FS probe result (%i)\n", reply->type);
        co_return;
    }

    if (reply->result_code != 0) {
        printf("Filesystem %s does not recognize the partition\n", fs->name.c_str());
        co_return;
    }

    std::vector<FSProperty> properties;

    try {
        properties = parse_fs_properties(std::span<const uint8_t>(reply->properties, msg->descriptor.size - sizeof(IPC_FS_Probe_Result)));
    } catch (const std::exception &e) {
        printf("Failed to parse filesystem properties: %s\n", e.what());
        co_return;
    }

    printf("Filesystem %s recognizes the partition! Properties:\n", fs->name.c_str());
    for (const auto &prop: properties) {
        std::visit(overloaded {
            [](const FSPropertyType &type) {
                printf("  Filesystem type: %s\n", type.name.c_str());
            },
            [](const FSPropertyUUID &uuid) {
                printf("  Filesystem UUID: %s\n", guid_to_string(uuid.uuid).c_str());
            },
            [](const FSPropertyLabel &label) {
                printf("  Filesystem label: %s\n", label.label.c_str());
            },
            [](const FSPropertyUnknown &unknown) {
                printf("  Unknown filesystem property: id=%" PRIu32 " flags=%" PRIu32 " data_size=%zu\n", unknown.property_id, unknown.property_flags, unknown.data.size());
            }
        }, prop);
    }

    fs->probed_partitions.insert(partition);
    partition->infos[fs->id] = std::move(properties);

    try_mount_new_fs(fs, partition);
}

void probe_new_partition(std::shared_ptr<Partition> p)
{
    for (auto fs: filesystems) {
        probe_partition(fs.second, p);
    }
}

pmos::async::detached_task probe_partitions(size_t disk_idx)
{
    auto &disk  = disks[disk_idx];
    // Read partition table
    auto object_right = co_await read_disk(disk, 0, 2);
    printf("Mem object right: %" PRIu64 "\n", object_right.get());

    if (!object_right) {
        printf("Failed to read partition table off disk %s\n", disk.name.c_str());
        co_return;
    }

    auto mbr_size          = align_to_page(disk.logical_sector_size * 2);

    map_mem_object_param_t p = {
        .page_table_id = PAGE_TABLE_SELF,
        .object_right = object_right.get(),
        .addr_start_uint = 0,
        .size = mbr_size,
        .offset_object = 0,
        .offset_start = 0,
        .object_size = mbr_size,
        .access_flags = PROT_READ,
    };
    auto r = map_mem_object(&p);

    auto result = r.result;
    auto mbr_ptr = r.virt_addr;
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
        auto gpt_object_right = co_await read_disk(disk, gpt->partition_entry_lba, number_of_sectors);
        if (!gpt_object_right) {
            printf("Failed to read GPT partition array\n");
            co_return;
        }
        auto array_size_aligned = align_to_page(gpt_partition_array_size);

        auto res = [&] { 
            map_mem_object_param_t p = {
                .page_table_id = 0,
                .object_right = gpt_object_right.get(),
                .addr_start_uint = 0,
                .size = array_size_aligned,
                .offset_object = 0,
                .offset_start = 0,
                .object_size = array_size_aligned,
                .access_flags = PROT_READ,
            };

            return map_mem_object(&p);
        }();

        auto result = res.result;
        auto gpt_ptr = res.virt_addr;
        if (result != SUCCESS) {
            printf("Failed to map GPT partition array %i (%s)\n", -(int)result, strerror(-result));
            co_return;
        }
        pmos::utility::scope_guard guard4 {[&] { munmap(gpt_ptr, array_size_aligned); }};


        std::vector<std::shared_ptr<Partition>> partitions;
        for (size_t offset = 0; offset < gpt_partition_array_size; offset += gpt_entry_size) {
            auto *entry = reinterpret_cast<GPTPartitionEntry *>(reinterpret_cast<char *>(gpt_ptr) + offset);
            if (guid_zero(entry->type_guid))
                continue;

            std::string guid = guid_to_string(entry->type_guid);
            printf("GPT partition: type %s, start %" PRIu64 ", end %" PRIu64 " offset %" PRIu64 "\n", guid.c_str(),
                   entry->first_lba, entry->last_lba, offset/gpt_entry_size);
            partitions.push_back(std::make_shared<Partition>(entry->first_lba, entry->last_lba));
        }
        disk.partitions = std::move(partitions);
    } else {
        printf("MBR partition table:\n");
        std::vector<std::shared_ptr<Partition>> partitions;
        for (int i = 0; i < 4; ++i) {
            auto &part = mbr->partitions[i];
            if (part.type == 0)
                continue;

            printf("Partition %i: type %x, start %" PRIu32 ", size %" PRIu32 "\n", i, part.type,
                   part.lba_start, part.num_sectors);

            partitions.push_back(std::make_shared<Partition>(part.lba_start, part.lba_start + part.num_sectors));
        }
        disk.partitions = std::move(partitions);
    }

    co_await create_partition_rights(disk_idx);

    for (auto p: disk.partitions) {
        ready_partitions.insert(p);
        probe_new_partition(p);
    }
}

pmos::async::task<void> populate_disk_info(Disk &disk, const BUSObject &object)
{
    // TODO: Query the driver if the object doesn't have the necessary properties

    auto extract_uint64_t = [&](const char *property_name) -> std::optional<uint64_t> {
        return object.get_property(property_name).and_then([&](const auto &prop) -> std::optional<uint64_t> {
            if (auto *val = std::get_if<uint64_t>(&prop))
                return *val;
            return std::nullopt;
        });
    };

    auto sector_count = extract_uint64_t("sector_count");
    auto logical_sector_size = extract_uint64_t("logical_sector_size");
    auto physical_sector_size = extract_uint64_t("physical_sector_size");

   if (!sector_count || !logical_sector_size || !physical_sector_size)
        throw std::runtime_error("Disk object is missing necessary properties");

    disk.sector_count = *sector_count;
    disk.logical_sector_size = *logical_sector_size;
    disk.physical_sector_size = *physical_sector_size;

    co_return;
}

pmos::async::detached_task register_disk(pmos::Right right, pmos::ipc::BUSObject object)
{
    auto idx = prepare_unused_disk_index();
    auto &disk = disks[idx];
    disk.used = true;
    disk.disk_right = std::move(right);
    disk.name = object.get_name();

    co_await populate_disk_info(disk, object);

    probe_partitions(idx);
    co_return;
}

void register_filesystem(pmos::Right right, pmos::ipc::BUSObject object)
{
    auto filesystem = std::make_shared<Filesystem>();
    filesystem->service_right = std::move(right);
    filesystem->name = object.get_name();

    filesystems.insert({filesystem->id, filesystem});

    for (auto p: ready_partitions) {
        probe_partition(filesystem, p);
    }
}

pmos::async::detached_task get_disks()
{
    auto filter = pmos::ipc::EqualsFilter("device", "hard_disk");
    uint64_t next_id = 0;
    while (true) {
        try {
            auto disk = co_await bus_helper.get_object(filter, next_id);
            if (disk) {
                next_id = disk.value().sequence_number + 1;
                printf("Got disk object with name %s\n", disk.value().object.get_name().c_str());

                register_disk(std::move(disk.value().right), std::move(disk.value().object));
            } else {
                printf("No more disk objects\n");
                break;
            }
        } catch (std::system_error &e) {
            printf("Error getting disk objects: %s\n", e.what());
            exit(1);
        }
    }
}

pmos::async::detached_task get_filesystems()
{
    auto filter = pmos::ipc::Conjunction({
        pmos::ipc::EqualsFilter("type", "service"),
        pmos::ipc::EqualsFilter("service_type", "filesystem"),
        pmos::ipc::EqualsFilter("filesystem_ops", "probe"),
        pmos::ipc::EqualsFilter("filesystem_ops", "mount"),
    });
    uint64_t next_id = 0;
    while (true) {
        auto fs = co_await bus_helper.get_object(filter, next_id);
        if (fs) {
            next_id = fs.value().sequence_number + 1;

            printf("Got a filesystem service with name %s\n", fs.value().object.get_name().c_str());
            register_filesystem(std::move(fs.value().right), std::move(fs.value().object));
        }
    }
}

int main()
{
    get_disks();
    get_filesystems();
    dispatcher.dispatch();
    return 0;
}