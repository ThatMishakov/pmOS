#pragma once
#include "ata.hh"

#include <stdexcept>
#include <pmos/async/coroutines.hh>
#include <pmos/containers/intrusive_bst.hh>
#include <pmos/containers/intrusive_list.hh>
#include <stdint.h>
#include <vector>

struct CommandListEntry;

struct CmdPortWaiter {
    pmos::containers::DoubleListHead<CmdPortWaiter> l;
    // Use callback to avoid UB
    void (*wait_callback)(CmdPortWaiter *self) = nullptr;
};

inline static constexpr uint32_t FIS_RECIEVE_AREA_SIZE = 0x1000;
inline static constexpr uint32_t PRDT_OFFSET           = 0x80;
inline static constexpr uint32_t PRDT_SIZE_BYTES       = 0x10;
inline static constexpr uint32_t COMMAND_TABLE_ENTRIES = 8;
inline static constexpr uint32_t COMMAND_TABLE_SIZE    = // 0x100
    PRDT_OFFSET + PRDT_SIZE_BYTES * COMMAND_TABLE_ENTRIES;
inline static constexpr uint32_t SCRATCH_SIZE = 0x400;

inline static constexpr uint32_t AHCI_GHC_HR   = 1 << 0;
inline static constexpr uint32_t AHCI_GHC_IE   = 1 << 1;
inline static constexpr uint32_t AHCI_GHC_MRSM = 1 << 2;
inline static constexpr uint32_t AHCI_GHC_AE   = 1 << 31;

inline static constexpr uint32_t AHCI_CAP_S64A = 1 << 31;
inline static constexpr uint32_t AHCI_CAP_SSS  = 1 << 27;

inline static constexpr uint32_t AHCI_CAP2_BOH = 1 << 0;

inline static constexpr uint32_t AHCI_BOHC_BOS  = 1 << 0;
inline static constexpr uint32_t AHCI_BOHC_OOS  = 1 << 1;
inline static constexpr uint32_t AHCI_BOHC_SOOE = 1 << 2;
inline static constexpr uint32_t AHCI_BOHC_OOC  = 1 << 3;
inline static constexpr uint32_t AHCI_BOHC_BB   = 1 << 4;

inline static constexpr uint32_t SATA_SIG_ATA   = 0x00000101;
inline static constexpr uint32_t SATA_SIG_ATAPI = 0xEB140101;
inline static constexpr uint32_t SATA_SIG_PM    = 0x96690101;
inline static constexpr uint32_t SATA_SIG_SEMB  = 0xC33C0101;

inline static constexpr uint32_t AHCI_PORT_IS_INDEX = 4;
inline static constexpr uint32_t AHCI_PORT_IE_INDEX = 5;

inline static constexpr uint32_t AHCI_CMD_INDEX     = 6;
inline static constexpr uint32_t AHCI_TFD_INDEX     = 8;
inline static constexpr uint32_t AHCI_SSTS_INDEX    = 10;
inline static constexpr uint32_t AHCI_SCTL_INDEX    = 11;
inline static constexpr uint32_t AHCI_SERR_INDEX    = 12;
inline static constexpr uint32_t AHCI_CI_INDEX      = 14;

inline static constexpr uint32_t AHCI_PORT_IS_DPS = 1 << 5;

inline static constexpr uint32_t AHCI_TFD_INDEX_DRQ = 1 << 3;
inline static constexpr uint32_t AHCI_TFD_INDEX_BSY = 1 << 7;

inline static constexpr uint32_t AHCI_PORT_CMD_ST    = 1 << 0;
inline static constexpr uint32_t AHCI_PORT_CMD_SUD   = 1 << 1;
inline static constexpr uint32_t AHCI_PORT_CMD_POD   = 1 << 2;
inline static constexpr uint32_t AHCI_PORT_CMD_CLO   = 1 << 3;
inline static constexpr uint32_t AHCI_PORT_CMD_FRE   = 1 << 4;
inline static constexpr uint32_t AHCI_PORT_CMD_CCS   = 1 << 8;
inline static constexpr uint32_t AHCI_PORT_CMD_MSS   = 1 << 13;
inline static constexpr uint32_t AHCI_PORT_CMD_FR    = 1 << 14;
inline static constexpr uint32_t AHCI_PORT_CMD_CR    = 1 << 15;
inline static constexpr uint32_t AHCI_PORT_CMD_CPS   = 1 << 16;
inline static constexpr uint32_t AHCI_PORT_CMD_PMA   = 1 << 17;
inline static constexpr uint32_t AHCI_PORT_CMD_HPCP  = 1 << 18;
inline static constexpr uint32_t AHCI_PORT_CMD_MPSP  = 1 << 19;
inline static constexpr uint32_t AHCI_PORT_CMD_CPD   = 1 << 20;
inline static constexpr uint32_t AHCI_PORT_CMD_ESP   = 1 << 21;
inline static constexpr uint32_t AHCI_PORT_CMD_FBSCP = 1 << 22;
inline static constexpr uint32_t AHCI_PORT_CMD_APSTE = 1 << 23;
inline static constexpr uint32_t AHCI_PORT_CMD_ATAPI = 1 << 24;
inline static constexpr uint32_t AHCI_PORT_CMD_DLAE  = 1 << 25;
inline static constexpr uint32_t AHCI_PORT_CMD_ALPE  = 1 << 26;
inline static constexpr uint32_t AHCI_PORT_CMD_ASP   = 1 << 27;
inline static constexpr uint32_t AHCI_PORT_CMD_ICC   = 1 << 28;

struct TimerWaiter {
    pmos::containers::RBTreeNode<TimerWaiter> timer_node = {};

    virtual ~TimerWaiter()     = default;
    virtual void react_timer() = 0;
    uint64_t timer_time        = 0;
    void wait(int time_ms);
};

struct WaitCommandCompletion;

struct AHCIPort: TimerWaiter {
    int index;

    AHCIPort(int index): index(index) {}

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
    std::vector<WaitCommandCompletion *> callbacks;
    int active_cmd_slots = 0;

    pmos::containers::CircularDoubleList<CmdPortWaiter, &CmdPortWaiter::l> cmd_waiters_head = {
        .head = {(pmos::containers::DoubleListHead<CmdPortWaiter> *)&cmd_waiters_head,
                 (pmos::containers::DoubleListHead<CmdPortWaiter> *)&cmd_waiters_head}};

    State state = State::Unknown;

    uint64_t timer_max = 0;

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
    void react_timer();
    void port_reset_hard();
    void init_drive();
    void init_ata_device();
    void dump_state();

    void react_interrupt();

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

struct Command {
    static pmos::async::task<Command> prepare(AHCIPort &port);

    AHCIPort *parent = nullptr;
    int cmd_index    = -1;
    int prdt_index   = 0;

    void prdt_push(PRDT entry);

    Command() = default;
    Command(AHCIPort &parent): parent(&parent) {}

    Command(Command &&) noexcept;
    Command &operator=(Command &&) noexcept;

    Command(Command const &) = delete;
    ~Command() noexcept;

    enum class Result {
        Success,
        Timeout,
    };

    pmos::async::task<Result> execute(uint8_t command, int timeout_ms);
};

struct WaitCommandCompletion: TimerWaiter {
    AHCIPort *parent;
    int cmd_index;
    int timeout_ms;

    std::coroutine_handle<> h_;

    enum class UnblockedBy {
        Timer,
        Interrupt,
        Ready,
        Unknown,
    } unblocked_by = UnblockedBy::Unknown;

    inline WaitCommandCompletion(AHCIPort *parent, int cmd_index, int timeout_ms)
        : parent(parent), cmd_index(cmd_index), timeout_ms(timeout_ms)
    {
    }

    bool await_ready() noexcept;
    void await_suspend(std::coroutine_handle<> h);
    UnblockedBy await_resume();
    void react_timer();
    void interrupt();
};

using TimerTree = pmos::containers::RedBlackTree<
    TimerWaiter, &TimerWaiter::timer_node,
    detail::TreeCmp<TimerWaiter, uint64_t, &TimerWaiter::timer_time>>;

extern TimerTree::RBTreeHead timer_tree;
extern uint64_t next_timer_time;
