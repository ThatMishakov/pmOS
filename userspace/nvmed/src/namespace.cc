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

    pmos::dma_wmb();

    auto completion_entry = co_await admin_submission_queue->submit_command(cmd);
    if (!completion_entry.success()) {
        std::cout << "Identify controller command failed with status: " << completion_entry.status << "\n";
        exit(1);
    }

    pmos::dma_rmb();

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