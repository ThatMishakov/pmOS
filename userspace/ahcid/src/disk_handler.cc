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

extern pmos::PortDispatcher dispatcher;
extern pmos::Port cmd_port;

struct DiskGeometry
{
    uint64_t sector_count;
    size_t logical_sector_size;
    size_t physical_sector_size;
};

uint64_t align_to_page(uint64_t size) { return (size + 0xfff) & ~(uint64_t)0xfff; }

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
        printf("Failed to send disk read reply: %i (%s)\n", (int)r.error(), strerror(r.error()));
    }
}

void handle_disk_read_error(int result, pmos::Right &reply_right)
{
    try {
        handle_disk_read({}, -result, reply_right);
    } catch (const std::exception &e) {
        printf("Exception in handle_disk_read_error: %s\n", e.what());
    }
}

pmos::async::detached_task handle_disk_read(IPC_Disk_Read request, DiskGeometry geometry, AHCIPort &disk_port, pmos::Right reply_right)
{
    auto sector_start = request.start_sector;
    auto sector_count = request.sector_count;

    if (sector_count == 0) {
        handle_disk_read_error(-EINVAL, reply_right);
        co_return;
    }

    if (sector_start > geometry.sector_count || (sector_start > UINT64_MAX - sector_count) ||
        sector_start + sector_count > geometry.sector_count) {
        handle_disk_read_error(-E2BIG, reply_right);
        co_return;
    }

    auto size         = sector_count * geometry.logical_sector_size;
    auto size_aligned = align_to_page(size);

    auto res = pmos::create_mem_object_noexcept(size_aligned, CREATE_FLAG_DMA | CREATE_FLAG_ALLOW_DISCONTINUOUS);
    if (!res) {
        handle_disk_read_error(res.error(), reply_right);
        co_return;
    }

    auto object = std::move(res.value());
    // RAII takes care of cleaning up the object if we fail before replying

    try {
        // Casually try to read the disk
        uint64_t bytes_read = 0;

        auto [result, phys_addr] = get_page_phys_address_from_object(object.get(), 0, FLAG_MEM_OBJECT_ID_RIGHT);
        if (result != SUCCESS) {
            printf("Error reading page from object\n");
            handle_disk_read_error(-result, reply_right);
            co_return;
        }

        auto sector_size = geometry.logical_sector_size;

        auto page_size                = 4096; // TODO: don't hardcode this
        phys_addr_t current_phys_addr = phys_addr;
        uint64_t phys_addr_of_offset  = 0;
        while (bytes_read < size) {
            Command cmd = co_await Command::prepare(disk_port);

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
                             .interrupt_on_completion = 0});
                        pushed_max_offset = size;
                        break;
                    } else if (offset == phys_addr_of_offset) {
                        offset += page_size;
                    } else {
                        auto [result, phys_addr] =
                            get_page_phys_address_from_object(object.get(), offset, FLAG_MEM_OBJECT_ID_RIGHT);
                        if (result != SUCCESS) {
                            handle_disk_read_error(result, reply_right);
                            co_return;
                        }

                        bool too_much_for_prdt =
                            offset - phys_addr_of_offset + page_size > PRDT::MAX_BYTES;
                        // This part is questionable...
                        bool too_many_sectors =
                            (offset - start_offset) / geometry.logical_sector_size >= 0xffff;

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
                                       .interrupt_on_completion = 0});

                        current_phys_addr   = phys_addr;
                        phys_addr_of_offset = offset;
                        pushed_max_offset   = offset;
                        break;
                    }
                }
            }

            auto sectors_count = (pushed_max_offset - start_offset) / sector_size;
            auto start_sector  = start_offset / sector_size;

            cmd.sector       = start_sector + sector_start;
            cmd.sector_count = sectors_count;

            auto result = co_await cmd.execute(0x25, 30'000);
            if (result != Command::Result::Success) {
                handle_disk_read_error(-EIO, reply_right);
                co_return;
            }

            bytes_read = pushed_max_offset;
        }

        handle_disk_read(std::move(object), 0, reply_right);
    } catch (const std::system_error &e) {
        printf("Error reading disk: %s\n", e.what());
        handle_disk_read_error(-e.code().value(), reply_right);
    }
}

pmos::async::detached_task handle_ipc(AHCIPort &port, uint64_t sector_count, size_t logical_sector_size, size_t physical_sector_size)
{
    DiskGeometry geometry{sector_count, logical_sector_size, physical_sector_size};

    while (true) {
        auto msg = co_await dispatcher.get_message(port.port_recieve_right);
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
        // case IPC_Disk_Open_NUM: {
        //     auto msg = (IPC_Disk_Open *)request;

        //     handle_disk_open(desc, msg);
        // } break;
        case IPC_Disk_Read_NUM: {
            auto rmsg = (IPC_Disk_Read *)msg->data.data();

            handle_disk_read(*rmsg, geometry, port, std::move(msg->reply_right));
        } break;
        default:
            fprintf(stderr, "ahcid: Received message with unknown type %u! Ignoring.\n", header->type);
            break;
        }
    }

    co_return;
}