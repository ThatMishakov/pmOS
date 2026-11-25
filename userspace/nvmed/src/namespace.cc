#include "namespace.hh"
#include "common.hh"
#include "queue.hh"
#include "spec.hh"
#include <iostream>
#include <pmos/device_handler.hh>
#include <unistd.h>
#include <pmos/utility/scope_guard.hh>
#include <pmos/memory.h>
#include <sys/mman.h>
#include <memory>
#include <pmos/helpers.hh>
#include <pmos/pmbus_helper.hh>

extern pmos::PortDispatcher dispatcher;
extern pmos::PMBUSHelper pmbus_helper;

extern std::string serial_number;

bool handle_create_right(pmos::Right disk_right, int result, pmos::Right &reply_right)
{
    IPC_Disk_Create_Right_Reply reply = {
        .type = IPC_Disk_Create_Right_Reply_NUM,
        .flags = 0,
        .result_code = static_cast<int16_t>(result),
    };

    auto r = send_message_right_one(reply_right, reply, {}, true, std::move(disk_right));
    if (!r) {
        printf("Failed to send disk create right reply: %i (%s)\n", (int)r.error().first, strerror(r.error().first));
        return false;
    }
    return true;
}

void handle_create_right_error(int result, pmos::Right &reply_right)
{
    handle_create_right({}, -result, reply_right);
}

pmos::async::detached_task handle_ipc(std::shared_ptr<NVMeNamespace> ns, pmos::ReceiveRight port_right, uint64_t from_sector, uint64_t to_sector_count);
void handle_right_create(std::shared_ptr<NVMeNamespace> ns, IPC_Disk_Create_Right request, uint64_t from_sector, uint64_t to_sector_count, pmos::Right reply_right)
{
    if (request.sector_count == 0) {
        handle_create_right_error(-EINVAL, reply_right);
        return;
    }

    auto req_start = request.start_sector;
    auto req_count = request.sector_count;

    if (req_start > UINT64_MAX - req_count || req_start + req_count > to_sector_count) {
        handle_create_right_error(-E2BIG, reply_right);
        return;
    }

    if (from_sector > UINT64_MAX - req_start) {
        handle_create_right_error(-E2BIG, reply_right);
        return;
    }

    auto sector_start = req_start + from_sector;
    auto sector_count = req_count;

    if (from_sector > UINT64_MAX - to_sector_count) {
        handle_create_right_error(-E2BIG, reply_right);
        return;
    }

    auto constraint_end = from_sector + to_sector_count;

    if (sector_start > UINT64_MAX - sector_count || sector_start + sector_count > constraint_end) {
        handle_create_right_error(-E2BIG, reply_right);
        return;
    }

    auto e = dispatcher.get_port().create_right(pmos::RightType::SendMany);
    if (not e) {
        handle_create_right_error(-ENOMEM, reply_right);
        return;
    }

    handle_ipc(ns, std::move(e->second), sector_start, sector_count);
    handle_create_right(std::move(e->first), 0, reply_right);
}


void handle_disk_describe(std::shared_ptr<NVMeNamespace> ns, uint64_t to_sector_count, pmos::Right reply_right)
{
    IPC_Disk_Describe_Reply reply = {
        .type = IPC_Disk_Describe_Reply_NUM,
        .flags = 0,
        .result_code = 0,
        .sector_count = to_sector_count,
        .logical_sector_size = ns->block_size,
        .physical_sector_size = ns->block_size,
    };

    auto r = send_message_right_one(reply_right, reply, {}, true);
    if (!r) {
        printf("Failed to send disk describe reply: %i (%s)\n", (int)r.error().first, strerror(r.error().first));
    }
}

uint64_t align_to_page(uint64_t size) { 
    uint64_t page_size = getpagesize();
    return (size + page_size - 1) & ~(page_size - 1);
}

void handle_disk_read(pmos::Right memory_object, int result, pmos::Right &reply_right)
{
    // TODO: Memory object handling
    IPC_Disk_Read_Reply reply = {
        .type        = IPC_Disk_Read_Reply_NUM,
        .flags       = 0,
        .result_code = static_cast<int16_t>(result),
    };

    // printf("Sending reply to disk read status %i\n", result);
    auto r = send_message_right_one(reply_right, reply, {}, true, std::move(memory_object));
    if (!r) {
        printf("Failed to send disk read reply: %i (%s)\n", (int)r.error().first, strerror(r.error().first));
    }
}

void handle_disk_read_error(int result, pmos::Right &reply_right)
{
    handle_disk_read({}, -result, reply_right);
}

pmos::async::task<void> read_namespace(std::shared_ptr<NVMeNamespace> ns, uint64_t sector_start, uint64_t sector_count, pmos::Right reply_right)
{
    const uint64_t page_size = getpagesize();
    const uint64_t page_mask = page_size - 1;
    const uint64_t total_bytes = sector_count * ns->block_size;
    const uint64_t size_aligned = (total_bytes + page_mask) & ~page_mask;
    const uint64_t pages_count = size_aligned / getpagesize();

    uint64_t list_pages = 0;
    const uint64_t data_pages = pages_count;
    if (data_pages > 2) {
        const uint64_t entries_per_page = page_size / sizeof(uint64_t);
        const uint64_t data_entries = data_pages - 1;

        // k list pages hold k * (entries_per_page - 1) + 1 data entries.
        list_pages = 1 + (data_entries - 2) / (entries_per_page - 1);
    }

    void *region_addr = nullptr;
    if (list_pages > 0) {
        auto region = create_normal_region(0, nullptr, list_pages * page_size, PROT_READ | PROT_WRITE | CREATE_FLAG_DMA | CREATE_FLAG_ALLOW_DISCONTINUOUS);
        if ((long)region.result < 0) {
            handle_disk_read_error(-ENOMEM, reply_right);
            co_return;
        }
        region_addr = region.virt_addr;
    }
    pmos::utility::scope_guard cleanup_region([region_addr, list_pages, page_size]() {
        if (region_addr) {
            munmap(region_addr, list_pages * page_size);
        }
    });

    auto res = pmos::create_mem_object_noexcept(size_aligned, CREATE_FLAG_DMA | CREATE_FLAG_ALLOW_DISCONTINUOUS);
    if (!res) {
        handle_disk_read_error(res.error(), reply_right);
        co_return;
    }
    auto object = std::move(res.value());

    NVMeCommand cmd = {};
    cmd.opcode = 0x02; // Read
    cmd.command_id = 1;
    cmd.nsid = ns->id;
    cmd.cdw10 = sector_start & 0xFFFFFFFF;
    cmd.cdw11 = (sector_start >> 32) & 0xFFFFFFFF;
    cmd.cdw12 = sector_count - 1;

    auto first = get_page_phys_address_from_object(
        object.get(), 0, 0);
    if (first.result != SUCCESS) {
        handle_disk_read_error(first.result, reply_right);
        co_return;
    }
    cmd.prp1 = first.phys_addr;

    if (pages_count == 2) {
        // PRP2 points directly to the second data page.
        auto second = get_page_phys_address_from_object(
            object.get(), page_size, 0);
        if (second.result != SUCCESS) {
            handle_disk_read_error(second.result, reply_right);
            co_return;
        }
        cmd.prp2 = second.phys_addr;
    } else if (pages_count > 2) {
        auto *entries = static_cast<uint64_t *>(region_addr);
        const uint64_t entries_per_page = page_size / sizeof(uint64_t);

        auto first_list = get_page_phys_address(0, region_addr, 0);
        if (first_list.result != SUCCESS) {
            handle_disk_read_error(first_list.result, reply_right);
            co_return;
        }
        cmd.prp2 = first_list.phys_addr;

        uint64_t data_page = 1; // Page 0 is already covered by PRP1.

        for (uint64_t list_page = 0; list_page < list_pages; ++list_page) {
            auto *list = entries + list_page * entries_per_page;
            const bool has_next = list_page + 1 < list_pages;
            const uint64_t slots = entries_per_page - (has_next ? 1 : 0);

            for (uint64_t slot = 0; slot < slots && data_page < pages_count; ++slot, ++data_page) {
                auto page = get_page_phys_address_from_object(
                    object.get(), data_page * page_size, 0);
                if (page.result != SUCCESS) {
                    handle_disk_read_error(page.result, reply_right);
                    co_return;
                }
                list[slot] = page.phys_addr;
            }

            if (has_next) {
                auto *next_list = entries + (list_page + 1) * entries_per_page;
                auto next = get_page_phys_address(0, next_list, 0);
                if (next.result != SUCCESS) {
                    handle_disk_read_error(next.result, reply_right);
                    co_return;
                }
                list[entries_per_page - 1] = next.phys_addr;
            }
        }
    }

    dma_wmb();

    auto result = co_await data_submission_queue->submit_command(cmd);
    if (!result.success()) {
        handle_disk_read_error(-EIO, reply_right);
        co_return;
    }

    dma_rmb();

    handle_disk_read(std::move(object), 0, reply_right);
}

pmos::async::detached_task handle_disk_read(std::shared_ptr<NVMeNamespace> ns, IPC_Disk_Read request, uint64_t from_sector, uint64_t to_sector_count, pmos::Right reply_right)
{
    if (request.sector_count == 0) {
        handle_disk_read_error(-EINVAL, reply_right);
        co_return;
    }

    auto req_start = request.start_sector;
    auto req_count = request.sector_count;

    if (req_start > UINT64_MAX - req_count || req_start + req_count > to_sector_count) {
        handle_disk_read_error(-E2BIG, reply_right);
        co_return;
    }

    if (from_sector > UINT64_MAX - req_start) {
        handle_disk_read_error(-E2BIG, reply_right);
        co_return;
    }

    auto sector_start = req_start + from_sector;
    auto sector_count = req_count;

    if (from_sector > UINT64_MAX - to_sector_count) {
        handle_disk_read_error(-E2BIG, reply_right);
        co_return;
    }

    auto constraint_end = from_sector + to_sector_count;

    if (sector_start > UINT64_MAX - sector_count || sector_start + sector_count > constraint_end) {
        handle_disk_read_error(-E2BIG, reply_right);
        co_return;
    }

    co_await read_namespace(ns, sector_start, sector_count, std::move(reply_right));
}

pmos::async::detached_task handle_ipc(std::shared_ptr<NVMeNamespace> ns, pmos::ReceiveRight receive_right, uint64_t from_sector, uint64_t to_sector_count)
{
    while (true) {
        auto msg = co_await dispatcher.get_message(receive_right);
        if (!msg) {
            fprintf(stderr, "ahcid: Failed to get message for port! %i\n", msg.error());
            exit(1);
        }

        if (msg->data.size() < sizeof(IPC_Generic_Msg)) {
            fprintf(stderr, "ahcid: Received message too small for header! Ignoring.\n");
            continue;
        }

        auto header = reinterpret_cast<const IPC_Generic_Msg *>(msg->data.data());
        switch (header->type) {
        case IPC_Kernel_Receive_Right_Destroyed_NUM:
            break;
        case IPC_Disk_Create_Right_NUM: {
            if (msg->data.size() < sizeof(IPC_Disk_Create_Right)) {
                fprintf(stderr, "ahcid: Received message IPC_Disk_Create_Right with too small of size\n");
                break;
            }

            auto cmsg = (IPC_Disk_Create_Right *)msg->data.data();

            handle_right_create(ns, *cmsg, from_sector, to_sector_count, std::move(msg->reply_right));
        } break;
        case IPC_Disk_Describe_NUM: {
            if (msg->data.size() < sizeof(IPC_Disk_Describe)) {
                fprintf(stderr, "ahcid: Received message IPC_Disk_Describe with too small of size\n");
                break;
            }

            handle_disk_describe(ns, to_sector_count, std::move(msg->reply_right));
        } break;
        case IPC_Disk_Read_NUM: {
            if (msg->data.size() < sizeof(IPC_Disk_Read)) {
                fprintf(stderr, "ahcid: Received message IPC_Disk_Read with too small of size\n");
                break;
            }

            auto rmsg = (IPC_Disk_Read *)msg->data.data();

            handle_disk_read(ns, *rmsg, from_sector, to_sector_count, std::move(msg->reply_right));
        } break;
        default:
            fprintf(stderr, "NVMe: Received message with unknown type %u! Ignoring.\n", header->type);
            break;
        }
    }
}


pmos::async::task<void> publish_disk(std::shared_ptr<NVMeNamespace> ns, pmos::Right right)
{
    pmos::ipc::BUSObject object;
    object.set_name("system.disk." + serial_number + "_ns" + std::to_string(ns->id));
    object.set_property("device", "hard_disk");
    object.set_property("namespace_id", ns->id);
    object.set_property("sector_count", ns->size);
    object.set_property("logical_sector_size", ns->block_size);
    object.set_property("physical_sector_size", ns->block_size);

    std::cout << "Publishing NVMe namespace " << ns->id << "\n";

    co_await pmbus_helper.publish_object(std::move(object), std::move(right));
}

pmos::async::detached_task handle_namespace(uint32_t namespace_id)
{
    std::cout << "Identifying namespace " << namespace_id << "...\n";

    auto page_size = getpagesize();
    auto region = create_normal_region(0, nullptr, page_size, PROT_READ | PROT_WRITE | CREATE_FLAG_DMA);
    if ((long)region.result < 0) {
        std::cout << "Failed to allocate memory for NVMe identify controller data: " << (int)region.result << " (" << strerror(-region.result) << ")\n";
        exit(1);
    }
    pmos::utility::scope_guard cleanup_region([region, page_size]() {
        munmap(region.virt_addr, page_size);
    });

    auto phys_addr = get_page_phys_address(0, region.virt_addr, 0);
    if ((long)phys_addr.result < 0) {
        std::cout << "Could not get physical address for NVMe identify controller data: "
                    << (int)phys_addr.result << " (" << strerror(-phys_addr.result) << ")\n";
        exit(1);
    }

    NVMeCommand cmd = {};
    cmd.opcode = 0x06; // Identify
    cmd.command_id = 1;
    cmd.nsid = namespace_id;
    cmd.prp1 = phys_addr.phys_addr;
    cmd.cdw10 = 0; // Identify namespace

    dma_wmb();

    auto completion_entry = co_await admin_submission_queue->submit_command(cmd);
    if (!completion_entry.success()) {
        std::cout << "Identify controller command failed with status: " << completion_entry.status << "\n";
        exit(1);
    }

    dma_rmb();

    auto *ns = reinterpret_cast<IdentifyNamespace *>(region.virt_addr);
    auto flbas = ns->flbas & 0xf;
    auto lbaf = ns->lbaf[flbas];
    unsigned block_size = 1u << lbaf.ds;
    auto size = ns->nsze;

    std::cout << "Namespace " << namespace_id << ": size (blocks) = " << size << ", block size = " << block_size << "\n";

    auto ns_ptr = std::make_shared<NVMeNamespace>();
    ns_ptr->id = namespace_id;
    ns_ptr->size = size;
    ns_ptr->block_size = block_size;

    auto [r, port_right] = dispatcher.get_port().create_right(pmos::RightType::SendMany).value();
    handle_ipc(ns_ptr, std::move(port_right), 0, size);
    co_await publish_disk(ns_ptr, std::move(r));
}