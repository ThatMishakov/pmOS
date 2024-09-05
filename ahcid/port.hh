#pragma once
#include <stdexcept>
#include <pmos/containers/intrusive_bst.hh>
#include <pmos/containers/intrusive_list.hh>
#include <stdint.h>
#include <vector>

struct CommandListEntry;

struct CmdPortWaiter {
    pmos::containers::DoubleListHead<CmdPortWaiter> l;
    // Use callback to avoid UB
    void (* wait_callback)(CmdPortWaiter *self) = nullptr;
};

struct AHCIPort {
    int index;
    uint64_t dma_phys_base           = -1;
    volatile uint32_t *dma_virt_base = nullptr;

    enum class State {
        Unknown = 0,
        Idle,
        WaitingForIdle1,
        WaitingForIdle2,
        WaitingForReset,
        WaitingForReady,
    };

    std::vector<bool> cmd_bitmap;
    int active_cmd_slots = 0;

    pmos::containers::CircularDoubleList<CmdPortWaiter, &CmdPortWaiter::l> cmd_waiters_head = {
        .head = {(pmos::containers::DoubleListHead<CmdPortWaiter> *)&cmd_waiters_head,
                 (pmos::containers::DoubleListHead<CmdPortWaiter> *)&cmd_waiters_head}};

    State state = State::Unknown;

    pmos::containers::RBTreeNode<AHCIPort> timer_node = {};
    uint64_t timer_time                               = 0;
    uint64_t timer_max                                = 0;

    static constexpr uint32_t recieved_fis_offset = 0x0;
    // Put it here for FIS-based switching
    static constexpr uint32_t command_list_offset = 0x1000;

    // 1 command table per command list, with 8 entries max
    static constexpr uint32_t command_table_offset = 0x1000 + 0x400;

    volatile uint32_t *get_port_register();
    volatile CommandListEntry *get_command_list_ptr(int index);
    volatile uint32_t *get_command_table_entry(int index);
    uint64_t get_command_table_phys(int index);

    void init_port();
    void enable_port();
    void port_idle();
    void port_idle2();
    void wait(int time_ms);
    void react_timer();
    void port_reset_hard();
    void init_drive();
    void init_ata_device();
    void dump_state();

    void detect_drive();

    uint32_t scratch_offet();

    volatile void *scratch_virt();
    uint64_t scratch_phys();

    enum class DeviceType {
        None,
        ATA,
        ATAPI,
        PM,
        SEMB,
    };

    DeviceType classify_device();
};

extern int num_slots;