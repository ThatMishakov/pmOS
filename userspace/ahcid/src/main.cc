#include "ata.hh"
#include "blockd.hh"
#include "device.hh"
#include "disk_handler.hh"
#include "pci.hh"
#include "port.hh"

#include <algorithm>
#include <alloca.h>
#include <cassert>
#include <inttypes.h>
#include <memory>
#include <pmos/helpers.h>
#include <pmos/interrupts.h>
#include <pmos/ipc.h>
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <pmos/helpers.hh>
#include <pmos/pmbus_helper.hh>
#include <pmos/async/coroutines.hh>

pmos::Right device_right;

pmos::Port cmd_port = pmos::Port::create().value();
auto dispatcher = pmos::PortDispatcher(cmd_port);
auto pmbus_helper = pmos::PMBUSHelper(dispatcher);

std::unique_ptr<PCIDevice> ahci_controller = nullptr;
volatile uint32_t *ahci_virt_base          = nullptr;
uint32_t ahci_int_vec                      = 0;

static constexpr uint32_t AHCI_PORT_SSTS_DET_MASK = 0xf;

static constexpr uint32_t AHCI_PORT_SCTL_DET_MASK = 0xf;

static constexpr uint32_t AHCI_MAX_PORTS = 32;

pmos::async::task<void> reset_controller()
{
    printf("Resetting AHCI controller\n");

    // 10.4.3 of AHCI spec

    uint32_t ghc = ahci_virt_base[1];
    ghc |= AHCI_GHC_HR;
    ahci_virt_base[1] = ghc;

    // Give controller 1s to clear HR
    for (int i = 0; i < 10; ++i) {
        usleep(100000);
        if (!(ahci_virt_base[1] & AHCI_GHC_HR))
            break;
    }

    if (ahci_virt_base[1] & AHCI_GHC_HR) {
        printf("Failed to reset AHCI controller... Exiting\n");
        exit(1);
    }

    printf("AHCI controller reset\n");

    co_return;
}

int num_slots         = 0;
int interface_speed   = 0;
bool staggered_spinup = false;

struct CommandListStructure {
    // 4.2.2 of AHCI spec
    uint32_t descriptor_information;
    uint32_t PRDBC; // Physical Region Descriptor Byte Count
    uint32_t command_table_base_low;
    uint32_t command_table_base_high;

    uint32_t reserved[4];
};

uint32_t AHCIPort::scratch_offet() { return command_table_offset + COMMAND_TABLE_SIZE; }

volatile void *AHCIPort::scratch_virt()
{
    return reinterpret_cast<volatile void *>(dma_virt_base + scratch_offet() / sizeof(uint32_t));
}

uint64_t AHCIPort::scratch_phys() { return dma_phys_base + scratch_offet(); }

void AHCIPort::dump_state()
{
    printf(" --- Port %i ---\n", index);
    printf("Port %i state dma_phys: %#" PRIx64 " virt 0x%p\n", index, dma_phys_base, dma_virt_base);
    auto port = get_port_register();
    printf(" P%iCLB: %#x P%iCLBU: %#x\n", index, port[0], index, port[1]);
    printf(" P%iFB: %#x P%iFBU: %#x\n", index, port[2], index, port[3]);
    printf(" P%iIS: %#x P%iIE: %#x\n", index, port[4], index, port[5]);
    printf(" P%iCMD: %#x\n", index, port[6]);
    printf(" P%iTFD: %#x\n", index, port[8]);
    printf(" P%iSIG: %#x\n", index, port[9]);
    printf(" P%iSSTS: %#x\n", index, port[10]);
    printf(" P%iSCTL: %#x\n", index, port[11]);
    printf(" P%iSERR: %#x\n", index, port[12]);
    printf(" P%iSACT: %#x\n", index, port[13]);
    printf(" P%iCI: %#x\n", index, port[14]);
    printf(" P%iSNTF: %#x\n", index, port[15]);
    printf(" P%iFBS: %#x\n", index, port[16]);
    printf(" P%iDEVSLP: %#x\n", index, port[17]);
    printf(" --- End Port %i ---\n", index);
}

pmos::async::task<void> dump_controller()
{
    printf("AHCI controller registers\n");
    printf(" CAP: %#x\n", ahci_virt_base[0]);
    printf(" GHC: %#x\n", ahci_virt_base[1]);
    printf(" IS: %#x\n", ahci_virt_base[2]);
    printf(" PI: %#x\n", ahci_virt_base[3]);
    printf(" VS: %#x\n", ahci_virt_base[4]);
    printf(" CCC_CTL: %#x\n", ahci_virt_base[5]);
    printf(" CCC_PORTS: %#x\n", ahci_virt_base[6]);
    printf(" EM_LOC: %#x\n", ahci_virt_base[7]);
    printf(" EM_CTL: %#x\n", ahci_virt_base[8]);
    printf(" CAP2: %#x\n", ahci_virt_base[9]);
    printf(" BOHC: %#x\n", ahci_virt_base[10]);

    printf(" --- PCI config space ---\n");
    printf(" Status+Command: %#x\n", co_await ahci_controller->readl(0x4));
}

uint64_t AHCIPort::get_command_table_phys(int index)
{
    return dma_phys_base + command_table_offset + (uint64_t)index * COMMAND_TABLE_SIZE;
}

volatile CommandListEntry *AHCIPort::get_command_list_ptr(int index)
{
    return reinterpret_cast<volatile CommandListEntry *>(
        dma_virt_base + (command_list_offset + index * 0x20) / sizeof(uint32_t));
}

void AHCIPort::init_ata_device()
{
    // Start the port
    auto port = get_port_register();
    auto cmdp = port[AHCI_CMD_INDEX];
    cmdp |= AHCI_PORT_CMD_ST; // Start
    port[AHCI_CMD_INDEX] = cmdp;

    handle_device(*this);
}

void AHCIPort::init_drive()
{
    printf("Drive ready at port %i: ", index);
    auto type = classify_device();
    switch (type) {
    case DeviceType::ATA:
        printf("ATA device\n");
        init_ata_device();
        break;
    case DeviceType::ATAPI:
        printf("ATAPI device\n");
        break;
    case DeviceType::PM:
        printf("Port Multiplier\n");
        break;
    case DeviceType::SEMB:
        printf("Serial Enclosure Management Bridge\n");
        break;
    case DeviceType::None:
        printf("Unknown device\n");
        break;
    }
}

AHCIPort::DeviceType AHCIPort::classify_device()
{
    auto port = get_port_register();
    auto sig  = port[9];

    // According to Linux, only look at the LBA high and mid bits
    uint8_t lba_high = (sig >> 24) & 0xff;
    uint8_t lba_mid  = (sig >> 16) & 0xff;

    if (lba_high == 0x96 && lba_mid == 0x69) {
        return DeviceType::PM;
    } else if (lba_high == 0xc3 && lba_mid == 0x3c) {
        return DeviceType::SEMB;
    } else if (lba_high == 0x00 && lba_mid == 0x00) {
        return DeviceType::ATA;
    } else if (lba_high == 0xeb && lba_mid == 0x14) {
        return DeviceType::ATAPI;
    } else {
        return DeviceType::None;
    }

    return DeviceType::None;
}

void AHCIPort::detect_drive()
{
    // TODO: Move this to a separate ATA library/driver since it's doesn't directly have to do with
    // AHCI

    state     = State::WaitingForReady;
    timer_max = pmos_get_time(GET_TIME_NANOSECONDS_SINCE_BOOTUP).value + 30'000'000'000;
    wait(100);
}

volatile uint32_t *AHCIPort::get_port_register()
{
    return ahci_virt_base + (0x100 + index * 0x80) / sizeof(uint32_t);
}

using TimerTree = pmos::containers::RedBlackTree<
    TimerWaiter, &TimerWaiter::timer_node,
    detail::TreeCmp<TimerWaiter, uint64_t, &TimerWaiter::timer_time>>;

TimerTree::RBTreeHead timer_tree;
uint64_t next_timer_time = 0;

std::vector<AHCIPort> ports;
AHCIPort &find_port(int i)
{
    auto it = std::lower_bound(ports.begin(), ports.end(), i,
                               [](const auto &port, auto i) { return port.index < i; });

    if (it == ports.end() || it->index != i) {
        throw std::runtime_error("Port not found");
    }

    return *it;
}

void AHCIPort::react_timer()
{
    switch (state) {
    case State::WaitingForIdle1: {
        auto port = get_port_register();
        auto cmd  = port[AHCI_CMD_INDEX];
        if (!(cmd & AHCI_PORT_CMD_CR)) {
            port_idle2();
            return;
        }
    }

        if (timer_max < timer_time) {
            port_reset_hard();
        } else {
            wait(10);
        }
        break;
    case State::WaitingForIdle2: {
        auto port = get_port_register();
        auto cmd  = port[AHCI_CMD_INDEX];
        if (!(cmd & AHCI_PORT_CMD_FR)) {
            state = State::Idle;
            enable_port();
            return;
        }
    }

        if (timer_max < timer_time) {
            port_reset_hard();
        } else {
            wait(10);
        }
        break;
    case State::WaitingForReady: {
        auto port = get_port_register();
        auto tfd  = port[AHCI_TFD_INDEX];

        if (tfd & 0x89) {
            if (timer_max < timer_time) {
                auto serr           = port[AHCI_SERR_INDEX];
                auto cmd            = port[AHCI_CMD_INDEX];
                auto fis_base       = port[2];
                auto fis_base_upper = port[3];
                printf("Drive not ready timed out. TFD: %#x; SERR: %#x, CMD: %#x, FBU %#x FB %#x\n",
                       tfd, serr, cmd, fis_base_upper, fis_base);
            } else {
                wait(100);
            }
        } else {
            init_drive();
        }
    } break;
    default:
        break;
    }
}

void AHCIPort::init_port()
{
    auto port = get_port_register();

    printf("Port %i cmd: 0x%x\n", index, port[AHCI_CMD_INDEX]);

    auto cmd = port[AHCI_CMD_INDEX];
    // "If PxCMD.ST, PxCMD.CR, PxCMD.FRE and PxCMD.FR are all cleared, the port is in an idle state"
    if ((cmd & (AHCI_PORT_CMD_ST | AHCI_PORT_CMD_CR | AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_FR)) == 0) {
        state = State::Idle;

        enable_port();
        return;
    }

    printf("Port %i is not in idle state. P%iCMD: 0x%x\n", index, index, cmd);

    // Switch to idle state
    port_idle();
}

void AHCIPort::enable_port()
{
    // AHCI spec 10.1.2

    auto addr = get_port_register();

    // Set command list base
    auto cmd_list_base = dma_phys_base + FIS_RECIEVE_AREA_SIZE;
    addr[0]            = cmd_list_base & 0xffffffff;
    addr[1]            = cmd_list_base >> 32;

    // Set FIS base
    addr[2] = dma_phys_base & 0xffffffff;
    addr[3] = dma_phys_base >> 32;

    // Set PxCMD.FRE to 1
    auto cmd = addr[AHCI_CMD_INDEX];
    cmd |= AHCI_PORT_CMD_FRE;
    addr[AHCI_CMD_INDEX] = cmd;

    // Enable interrupts
    // Clear PxIS
    addr[4] = 0xffffffff;
    // Enable all interrupts
    addr[5] = 0xffffffff;

    printf("Port %i in minimal configuration\n", index);

    if (staggered_spinup) {
        cmd |= AHCI_PORT_CMD_SUD;
        addr[AHCI_CMD_INDEX] = cmd;
    }

    state = State::Idle;

    // SATA spec 7.2.2:
    // "wait up to 10 ms for SStatus.DET field = 3h ..."
    usleep(10'000);

    auto det = addr[10] & AHCI_PORT_SSTS_DET_MASK;
    if (det == 0) {
        printf("No device detected on port %i\n", index);
        return;
    }
    printf("Port %i SStatus.DET: %#x\n", index, det);

    // Clear PxSERR
    addr[AHCI_SERR_INDEX] = 0xffffffff;

    printf("Device detected on port %i\n", index);
    detect_drive();
}

void AHCIPort::port_reset_hard()
{
    auto port = get_port_register();

    // "In systems that support staggered spin-up ... the  two register fields must be set correctly
    // in order to avoid illegal combinations of the two values."
    if (staggered_spinup) {
        // Set PxCMD.SUD to 1
        auto cmd = port[AHCI_CMD_INDEX];
        cmd |= AHCI_PORT_CMD_SUD;
        port[AHCI_CMD_INDEX] = cmd;
    }

    // "Software causes a port reset (COMRESET) by writing 1h to the PxSCTL.DET"
    auto sctl = port[11];
    sctl &= ~AHCI_PORT_SCTL_DET_MASK;
    sctl |= 1;
    port[11] = sctl;

    // "Software shall wait at least 1ms before clearing PxSCTL.DET to 0"
    usleep(1000);

    sctl &= ~AHCI_PORT_SCTL_DET_MASK;
    port[11] = sctl;

    enable_port();
}

void AHCIPort::port_idle()
{
    auto port = get_port_register();

    // Set port to idle
    auto cmd = port[AHCI_CMD_INDEX];
    cmd &= ~AHCI_PORT_CMD_ST;
    port[AHCI_CMD_INDEX] = cmd;

    // Wait for 500ms
    state = State::WaitingForIdle1;

    auto time = pmos_get_time(GET_TIME_NANOSECONDS_SINCE_BOOTUP);
    timer_max = time.value + 500'000'000;
    wait(10);
}

void AHCIPort::port_idle2()
{
    auto port = get_port_register();

    // "If PxCMD.FRE is set to ‘1’, software should clear it to ‘0’ and wait at least 500
    // milliseconds for PxCMD.FR to return ‘0’ when read."
    auto cmd = port[6];
    if (cmd & AHCI_PORT_CMD_FRE) {
        cmd &= ~AHCI_PORT_CMD_FRE;
        port[AHCI_CMD_INDEX] = cmd;

        state = State::WaitingForIdle2;

        auto time = pmos_get_time(GET_TIME_NANOSECONDS_SINCE_BOOTUP);
        timer_max = time.value + 500'000'000;
        wait(10);
    } else {
        state = State::Idle;
        enable_port();
    }
}

pmos::RecieveRight timer_right;

void react_timer()
{
    auto current_time = pmos_get_time(GET_TIME_NANOSECONDS_SINCE_BOOTUP);
    auto it           = timer_tree.begin();
    while ((it != timer_tree.end()) && (it->timer_time < current_time.value)) {
        timer_tree.erase(it);
        it->react_timer();
        it = timer_tree.begin();
    }

    if (it != timer_tree.end()) {
        next_timer_time = it->timer_time;
        set_deadline(timer_right, next_timer_time);
    }
}

pmos::async::detached_task init_timer()
{
    timer_right = create_timer_right(cmd_port);

    while (1) {
        auto msg = co_await dispatcher.get_message(timer_right);
        if (!msg) {
            fprintf(stderr, "ahcid: Failed to get timer message: %i\n", msg.error());
            exit(1);
        }

        auto request = (IPC_Generic_Msg *)msg->data.data();
        switch (request->type) {
        case IPC_Timer_Expired_NUM:
            react_timer();
            break;
        default:
            printf("AHCId unknown message type: %i in timer handler!\n", request->type);
            break;
        }
    }
}

void TimerWaiter::wait(int time_ms)
{
    auto time = pmos_get_time(GET_TIME_NANOSECONDS_SINCE_BOOTUP);
    if (time.result != 0) {
        printf("Failed to get time\n");
        return;
    }

    timer_time = time.value + (uint64_t)time_ms * 1'000'000;
    timer_tree.insert(this);

    if (timer_tree.begin() == this) {
        next_timer_time = timer_time;
        set_deadline(timer_right, timer_time);
    } else {
        TimerTree::RBTreeIterator it;
        while (!timer_tree.empty() and ((it = timer_tree.begin())->timer_time < time.value)) {
            timer_tree.erase(it);
            it->react_timer();
        }
    }
}

void react_interrupt()
{
    uint32_t is       = ahci_virt_base[2];
    ahci_virt_base[2] = is;
    for (int i = 0; i < 32; ++i) {
        if (is & (1 << i)) {
            //printf("Interrupt on port %i\n", i);
            find_port(i).react_interrupt();
        }
    }
}

pmos::async::detached_task ahci_controller_main()
{
    while (1) {
        auto msg = co_await dispatcher.get_message_default();
        if (!msg) {
            fprintf(stderr, "ahcid: Failed to get message: %i\n", msg.error());
            exit(1);
        }

        auto request = (IPC_Generic_Msg *)msg->data.data();

        switch (request->type) {
        default:
            printf("AHCId unknown message type: %i\n", request->type);
            break;
        }
    }
}

std::string pci_string;

pmos::async::detached_task handle_interrupts(pmos::RecieveRight int_right)
{
    while (1) {
        auto msg = co_await dispatcher.get_message(int_right);
        if (!msg) {
            fprintf(stderr, "ahcid: Failed to get message: %i\n", msg.error());
            exit(1);
        }

        auto request = (IPC_Generic_Msg *)msg->data.data();
        switch (request->type) {
        case IPC_Kernel_Interrupt_NUM: {
            // auto kmsg = (IPC_Kernel_Interrupt *)request;
            //printf("Kernel interrupt: %i\n", kmsg->intno);
            react_interrupt();
            pmos::complete_interrupt(int_right);
        } break;
        default:
            printf("AHCId: unknown message type: %i in interrupt handler!\n", request->type);
            exit(1);
            break;
        }
    }
}

pmos::async::detached_task ahci_handle()
{
    // pci_string = "pci_" + std::to_string(d.group) + "_" + std::to_string(d.bus) + "_" + std::to_string(d.device) + "_" + std::to_string(d.function);
    ahci_controller = co_await get_pci_device();

    printf("AHCI controller created\n");

    uint16_t vendor_id = co_await ahci_controller->readw(0);
    uint16_t device_id = co_await ahci_controller->readw(2);
    uint8_t class_code = co_await ahci_controller->readb(0xb);
    uint8_t subclass   = co_await ahci_controller->readb(0xa);
    uint8_t prog_if    = co_await ahci_controller->readb(0x9);
    printf("AHCI controller: vendor 0x%x device 0x%x class 0x%x subclass 0x%x prog_if 0x%x\n",
           vendor_id, device_id, class_code, subclass, prog_if);

    // Enable DMA and memory space access
    uint32_t command = co_await ahci_controller->readw(0x4);
    command |= 0x06;
    co_await ahci_controller->writew(0x4, command);

    // Get BAR5
    uint32_t bar5 = co_await ahci_controller->readl(0x24);
    printf("ACHI BAR5: %#x\n", bar5);

    bar5 &= 0xfffffff0;

    auto mem_req = create_phys_map_region(0, nullptr, 8192, PROT_READ | PROT_WRITE, bar5);
    if (mem_req.result != SUCCESS) {
        printf("Failed to map AHCI controller's memory\n");
        exit(1);
    }
    ahci_virt_base = reinterpret_cast<volatile uint32_t *>(mem_req.virt_addr);

    uint32_t ghc = ahci_virt_base[1];
    // Enable AHCI
    ghc |= AHCI_GHC_AE;
    ahci_virt_base[1] = ghc;

    uint32_t cap = ahci_virt_base[0];
    printf("AHCI Capabilities: 0x%x\n", cap);

    bool s64a = !!(cap & (1 << 31));
    if (!s64a) {
        printf("AHCI controller does not support 64-bit addressing\n");
        exit(1);
    }

    uint32_t cap2 = ahci_virt_base[9];
    printf("AHCI Capabilities 2: %#x\n", cap2);

    uint32_t ahci_version = ahci_virt_base[4];
    printf("AHCI Version: %#x\n", ahci_version);

    // BIOS handoff
    if (cap2 & AHCI_CAP2_BOH) {
        // 10.6.3 of AHCI spec
        printf("BIOS handoff supported... performing it\n");

        // Set OS Ownership to 1
        uint32_t bohc      = ahci_virt_base[10];
        ahci_virt_base[10] = bohc | AHCI_BOHC_BOS;

        // Wait for 25ms for BOS to be cleared
        for (int i = 0; i < 25; ++i) {
            if (!(ahci_virt_base[10] & AHCI_BOHC_BOS))
                break;
            usleep(1000);
        }

        if (!(ahci_virt_base[10] & AHCI_BOHC_BOS)) {
            printf("BIOS handoff successful\n");
        } else if (ahci_virt_base[10] & AHCI_BOHC_BB) {
            printf("BIOS busy... waiting for it to finish\n");

            // Sleep for 2s in 100ms intervals
            for (int i = 0; i < 20; ++i) {
                usleep(100000);
                if (!(ahci_virt_base[10] & AHCI_BOHC_BB)) {
                    printf("BIOS handoff successful\n");
                    break;
                }
            }
            printf("BIOS timed out...\n");
        } else {
            printf("BIOS timed out...\n");
        }
    }

    // Reset the controller
    co_await reset_controller();

    // Enable ACPI again
    ghc = ahci_virt_base[1];
    ghc |= AHCI_GHC_AE;
    ahci_virt_base[1] = ghc;

    cap = ahci_virt_base[0];

    num_slots        = (cap >> 8) & 0x1f;
    interface_speed  = (cap >> 20) & 0xf;
    staggered_spinup = !!(cap & AHCI_CAP_SSS);
    if (staggered_spinup) {
        printf("Staggered spinup supported\n");
    }

    printf("AHCI controller initialized\n");

    // TODO: Initialize ports
    uint32_t pi = ahci_virt_base[3];
    printf("AHCI Ports Implemented: 0x%x\n", pi);

    for (unsigned i = 0; i < AHCI_MAX_PORTS; ++i) {
        if (pi & (1 << i)) {
            printf("Port %i implemented\n", i);
            ports.push_back(AHCIPort((int)i));
            // Clear interrupts while we're at it
            auto port = ports.back().get_port_register();
            port[4]   = 0xffffffff;
        }
    }

    // Attach PCI interrupt
    auto r = co_await ahci_controller->register_interrupt();
    handle_interrupts(std::move(r));

    printf("Interrupt registered...\n");

    {
    // Clear 'interrupt disable' bit
    auto command = co_await ahci_controller->readw(0x4);
    command &= ~(1 << 10);
    printf("Command: %#x\n", command);
    co_await ahci_controller->writew(0x4, command);

    // Clear interrupt status
    ahci_virt_base[2] = 0xffffffff;

    // Enable interrupts
    ghc = ahci_virt_base[1];
    ghc |= AHCI_GHC_IE;
    ahci_virt_base[1] = ghc;
    }

    for (auto &port: ports) {
        size_t mem_size = port.command_table_offset + num_slots * COMMAND_TABLE_SIZE + SCRATCH_SIZE;
        // TODO: Align to page size constants
        mem_size        = (mem_size + 0xfff) & ~0xfff;

        auto request =
            create_normal_region(0, nullptr, mem_size, PROT_READ | PROT_WRITE | CREATE_FLAG_DMA);
        if ((long)request.result < 0) {
            printf("Failed to allocate memory for AHCI: %i (%s)\n", (int)request.result,
                   strerror(-request.result));
            exit(1);
        }

        memset(reinterpret_cast<void *>(request.virt_addr), 0, mem_size);

        auto phys_addr = get_page_phys_address(0, request.virt_addr, 0);
        if ((long)phys_addr.result < 0) {
            printf("Could not get physical address for AHCI: %i (%s)\n", (int)phys_addr.result,
                   strerror(-request.result));
            exit(1);
        }

        port.dma_phys_base = phys_addr.phys_addr;
        port.dma_virt_base = reinterpret_cast<volatile uint32_t *>(request.virt_addr);

        printf("Allocated memory for port %i: %#" PRIx64 " - %#" PRIx64 " -> 0x%p\n", port.index,
               port.dma_phys_base, port.dma_phys_base + mem_size, port.dma_virt_base);

        for (int i = 0; i < num_slots; ++i) {
            auto &command_list = reinterpret_cast<volatile CommandListStructure *>(
                port.dma_virt_base + port.command_list_offset / sizeof(uint32_t))[i];
            command_list.descriptor_information = 0x80000000;
            command_list.PRDBC                  = 0;
            uint64_t command_table_phys_base =
                port.dma_phys_base + port.command_table_offset + i * COMMAND_TABLE_SIZE;
            command_list.command_table_base_low  = command_table_phys_base & 0xffffffff;
            command_list.command_table_base_high = command_table_phys_base >> 32;
        }

        port.callbacks  = std::vector<WaitCommandCompletion *>(num_slots, nullptr);
        port.cmd_bitmap = std::vector<bool>(num_slots, false);

        port.init_port();
    }

    ahci_controller_main();
}

void usage(char *name)
{
    printf("ahcid Usage: %s --right-id <ID of the right to the i8042 controller>\n", name);
    exit(1);
}

void parse_args(int argc, char **argv)
{
    if (argc < 1)
        printf("ahcid: empty argc!\n");
    char *name = argv[0];

    bool have_right = false;

    for (int i = 1; i < argc; ++i) {
        char *r = argv[i];

        if (!strcmp(r, "--right-id")) {
            if (have_right) {
                printf("ahcid: Repeated --right-id\n");
                usage(name);
            }

            ++i;
            if (i >= argc) {
                printf("ahcid missing right ID\n");
                usage(name);
            }
            pmos_right_t right_to_device = strtoull(argv[i], NULL, 0);
            have_right = true;
            device_right = pmos::Right{right_to_device, pmos::RightType::SendMany};
        } else {
            printf("Unrecognized argument %s\n", argv[i]);
            usage(name);
        }
    }

    if (!have_right) {
        printf("ahcid: no right_id!\n");
        usage(name);
    }
}

int main(int argc, char **argv)
{
    //printf("Hello from AHCId! My PID: %" PRIi64 "\n", getpid());
    printf("AHCId started...\n");
    parse_args(argc, argv);

    init_timer();
    ahci_handle();
    ahci_controller_main();

    dispatcher.dispatch();

    printf("!!! ahcid: Exiting... !!!\n");
}