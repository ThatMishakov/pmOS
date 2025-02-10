#include "device.hh"
#include "misc.hh"

#include "port.hh"

#include <cassert>
#include <stdio.h>
#include <utility>
#include <pmos/memory.h>
#include <sys/mman.h>
#include <pmos/utility/scope_guard.hh>
#include "blockd.hh"
#include <cinttypes>

pmos::async::detached_task handle_device(AHCIPort &parent)
{
    constexpr size_t dma_size = 4096;
    auto req = create_normal_region(0, nullptr, dma_size, PROT_READ | PROT_WRITE | CREATE_FLAG_DMA);
    auto guard_ = pmos::utility::make_scope_guard([&] { munmap(req.virt_addr, dma_size); });
    auto phys_addr = get_page_phys_address(0, req.virt_addr, 0);
    if (phys_addr.result < 0) {
        printf("Could not get physical address for AHCI: %i\n", (int)phys_addr.result);
        co_return;
    }
    memset(req.virt_addr, 0, dma_size);

    Command cmd = co_await Command::prepare(parent);
    cmd.prdt_push({.data_base               = phys_addr.phys_addr,
                   .rsv0                    = 0,
                   .data_base_count         = sizeof(IDENTIFYData) - 1,
                   .rsv1                    = 0,
                   .interrupt_on_completion = 0});

    auto result = co_await cmd.execute(0xEC, 30'000);

    if (result != Command::Result::Success) {
        printf("Drive timed out\n");
        co_return;
    }

    auto *s = reinterpret_cast<IDENTIFYData *>(req.virt_addr);
    auto model = s->get_model();
    printf("Port %i hard drive model: %s\n", parent.index, model.c_str());

    auto [logical_sector_size, physical_sector_size] = s->get_sector_size();

    printf("Port %i logical sector size: %i, physical sector size: %i\n", parent.index,
           logical_sector_size, physical_sector_size);

    auto sector_count = s->get_sector_count();

    printf("Port %i sector count: %" PRIu64 "\n", parent.index, sector_count);

    auto disk = co_await register_disk(parent, sector_count, logical_sector_size, physical_sector_size);
    
    printf("Port %i disk registered: %" PRIu64 "\n", parent.index, disk);

    co_return;
}

bool GetCmdIndex::await_ready() const noexcept { return parent.active_cmd_slots < num_slots; }

void GetCmdIndex::await_suspend(std::coroutine_handle<> h)
{
    h_ = h;
    parent.cmd_waiters_head.push_back(this);
}

void GetCmdIndex::available_callback(CmdPortWaiter *self)
{
    auto s = static_cast<GetCmdIndex *>(self);
    s->h_();
}

int GetCmdIndex::await_resume()
{
    assert(parent.active_cmd_slots < num_slots);
    assert((size_t)parent.cmd_bitmap.size() == (size_t)num_slots);

    for (int i = 0; i < num_slots; ++i)
        if (not parent.cmd_bitmap[i]) {
            parent.cmd_bitmap[i] = true;
            return i;
        }

    assert(!"parent.active_cmd_slots accounting mismatch with parent.cmd_bitmap");
    return -1;
}