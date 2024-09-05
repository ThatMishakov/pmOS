#include "device.hh"
#include "port.hh"
#include <stdio.h>
#include <cassert>

AHCIDeviceReturnObject handle_device(AHCIPort &parent)
{
    printf("Hello from handle_device!\n");

    auto cmd_index = co_await GetCmdIndex(parent);
    printf("cmd_index: %i\n", cmd_index);

    // // Start the port
    // auto port = parent.get_port_register();
    // auto cmdp = port[AHCI_CMD_INDEX];
    // cmdp |= AHCI_PORT_CMD_ST; // Start
    // port[AHCI_CMD_INDEX] = cmdp;

    // // Request IDENTIFY DEVICE

    // // Send IDENTIFY command
    // FIS_Host_To_Device str = {};
    // str.fis_type           = FIS_TYPE_REG_H2D;
    // str.command            = 0xEC; // IDENTIFY DEVICE
    // str.c                  = 1;

    // auto *fis = (FIS_Host_To_Device *)(dma_virt_base + command_table_offset / sizeof(uint32_t));
    // *fis      = str;

    // auto *prdt = (PRDT *)(fis + PRDT_OFFSET / sizeof(uint32_t));
    // *prdt      = {.data_base               = scratch_phys(),
    //               .rsv0                    = 0,
    //               .data_base_count         = 512 - 1,
    //               .rsv1                    = 0,
    //               .interrupt_on_completion = 1};

    // CommandListEntry cmd   = {};
    // cmd.command_fis_length = sizeof(FIS_Host_To_Device) / sizeof(uint32_t);
    // cmd.command_table_base = get_command_table_phys(0);
    // cmd.prdt_length        = 1;
    // cmd.clear_busy         = 1;
    // auto list              = (CommandListEntry *)get_command_list_ptr(0);
    // *list                  = cmd;

    // // Issue command
    // port[AHCI_CI_INDEX] = 1 << 0;

    // printf("Command issued. CI: %#x\n", port[AHCI_CI_INDEX]);

    // sleep(1);

    // printf("Command completed. CI: %#x\n", port[AHCI_CI_INDEX]);

    co_return;
}

bool GetCmdIndex::await_ready() const noexcept
{
    return parent.active_cmd_slots < num_slots;
}

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
    // How does this generate a warning?
    return -1;
}