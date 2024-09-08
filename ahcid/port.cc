#include "port.hh"

#include "device.hh"

#include <cassert>
#include <stdio.h>

pmos::async::task<Command> Command::prepare(AHCIPort &port)
{
    Command cmd(port);
    cmd.cmd_index = co_await GetCmdIndex {port};
    co_return cmd;
}

Command::~Command() noexcept
{
    if (parent && cmd_index != -1) {
        parent->active_cmd_slots &= ~(1 << cmd_index);
        parent->active_cmd_slots--;
    }
}

Command::Command(Command &&other) noexcept
{
    parent           = other.parent;
    cmd_index        = other.cmd_index;
    prdt_index       = other.prdt_index;
    other.parent     = nullptr;
    other.cmd_index  = -1;
    other.prdt_index = 0;
}

Command &Command::operator=(Command &&other) noexcept
{
    if (this == &other)
        return *this;

    if (parent && cmd_index != -1) {
        parent->active_cmd_slots &= ~(1 << cmd_index);
        parent->active_cmd_slots--;
    }

    parent           = other.parent;
    cmd_index        = other.cmd_index;
    prdt_index       = other.prdt_index;
    other.parent     = nullptr;
    other.cmd_index  = -1;
    other.prdt_index = 0;
    return *this;
}

void Command::prdt_push(PRDT entry)
{
    assert(prdt_index < 8);
    auto *prdt = (PRDT *)(parent->dma_virt_base + (parent->command_table_offset + PRDT_OFFSET +
                                                   cmd_index * COMMAND_TABLE_SIZE) /
                                                      sizeof(uint32_t));
    prdt[prdt_index++] = entry;
}

pmos::async::task<Command::Result> Command::execute(uint8_t command, int timeout_ms)
{
    FIS_Host_To_Device str = {};
    str.fis_type           = FIS_TYPE_REG_H2D;
    str.command            = command;
    str.device             = 1 << 6;
    str.c                  = 1;

    auto *fis = (FIS_Host_To_Device *)(parent->dma_virt_base +
                                       parent->command_table_offset / sizeof(uint32_t) +
                                       cmd_index * COMMAND_TABLE_SIZE / sizeof(uint32_t));
    *fis      = str;

    CommandListEntry cmd   = {};
    cmd.command_fis_length = sizeof(FIS_Host_To_Device) / sizeof(uint32_t);
    cmd.command_table_base = parent->get_command_table_phys(cmd_index);
    cmd.prdt_length        = prdt_index;
    auto list              = (CommandListEntry *)parent->get_command_list_ptr(cmd_index);
    *list                  = cmd;

    auto port = parent->get_port_register();

    // Wait for the port to be ready
    while (port[AHCI_TFD_INDEX] & (AHCI_TFD_INDEX_DRQ | AHCI_TFD_INDEX_BSY))
        sched_yield();

    port[AHCI_CI_INDEX] = 1 << cmd_index;

    co_await WaitCommandCompletion(parent, cmd_index, timeout_ms);

    if (port[AHCI_CI_INDEX] & (1 << cmd_index)) {
        co_return Result::Timeout;
    } else {
        co_return Result::Success;
    }
}

bool WaitCommandCompletion::await_ready() noexcept
{
    bool ready = !((parent->get_port_register()[AHCI_CI_INDEX]) & (1 << cmd_index));
    if (ready)
        unblocked_by = UnblockedBy::Ready;
    return ready;
}

void WaitCommandCompletion::await_suspend(std::coroutine_handle<> h)
{
    h_ = h;
    wait(timeout_ms);
    parent->callbacks[cmd_index] = this;
}

void WaitCommandCompletion::react_timer()
{
    unblocked_by                 = UnblockedBy::Timer;
    parent->callbacks[cmd_index] = nullptr;
    h_.resume();
}

void WaitCommandCompletion::interrupt()
{
    unblocked_by                 = UnblockedBy::Interrupt;
    parent->callbacks[cmd_index] = nullptr;
    timer_tree.erase(this);
    h_.resume();
}

WaitCommandCompletion::UnblockedBy WaitCommandCompletion::await_resume() { return unblocked_by; }

void AHCIPort::react_interrupt()
{
    auto port    = get_port_register();
    auto port_is = port[4];
    port[4]      = port_is;

    printf("Port %i interrupt status: %#x\n", index, port_is);
    auto command = port[AHCI_CI_INDEX];
    for (int i = 0; i < num_slots; ++i) {
        if (!(command & (1 << i)) && callbacks[i] && cmd_bitmap[i]) {
            callbacks[i]->interrupt();
        }
    }
}