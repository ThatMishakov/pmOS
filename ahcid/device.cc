#include "device.hh"

#include "port.hh"

#include <cassert>
#include <stdio.h>
#include <utility>

pmos::async::detached_task handle_device(AHCIPort &parent)
{
    Command cmd = co_await Command::prepare(parent);
    cmd.prdt_push({.data_base               = parent.scratch_phys(),
                   .rsv0                    = 0,
                   .data_base_count         = 512 - 1,
                   .rsv1                    = 0,
                   .interrupt_on_completion = 1});

    auto result = co_await cmd.execute(0xEC, 30'000);

    if (result == Command::Result::Success) {
        printf("Device is ready\n");
    } else {
        printf("Device timed out\n");
    }

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

    for (int i = 0; i < num_slots; ++i)
        if (not parent.cmd_bitmap[i]) {
            parent.cmd_bitmap[i] = true;
            return i;
        }

    assert(!"parent.active_cmd_slots accounting mismatch with parent.cmd_bitmap");
    return -1;
}