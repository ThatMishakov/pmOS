#include <pmos/device_handler.hh>

#include <algorithm>
#include <cassert>
#include <inttypes.h>
#include <memory>
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
#include <sys/mman.h>
#include <cstdlib>
#include <time.h>
#include <algorithm>
#include <iostream>
#include <pmos/io.h>

using namespace pmos;

pmos::Right device_right;

pmos::Port cmd_port = pmos::Port::create().value();
auto dispatcher = pmos::PortDispatcher(cmd_port);
auto pmbus_helper = pmos::PMBUSHelper(dispatcher);

std::unique_ptr<pmos::DeviceHandler> nvme_controller = nullptr;

uint32_t *nvme_virt_base          = nullptr;

// TimerTree::RBTreeHead timer_tree;
uint64_t next_timer_time = 0;

pmos::ReceiveRight timer_right;

pmos::async::detached_task handle_interrupts(pmos::ReceiveRight int_right)
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
            // printf("Kernel interrupt: %i\n", kmsg->intno);
            // react_interrupt();
            pmos::complete_interrupt(int_right);
        } break;
        default:
            printf("NVMe: unknown message type: %i in interrupt handler!\n", request->type);
            exit(1);
            break;
        }
    }
}

constexpr uint32_t NVME_REG_CAP = 0x0;
constexpr uint32_t NVME_REG_VS  = 0x8;
constexpr uint32_t NVME_REG_INTMS = 0xC;
constexpr uint32_t NVME_REG_INTMC = 0x10;
constexpr uint32_t NVME_REG_CC = 0x14;
constexpr uint32_t NVME_REG_CSTS = 0x1C;

uint32_t doorbell_stride = 0; // register * (1 << doorbell_stride)
uint32_t timeout = 0; // 500 milliseconds units
uint32_t maximum_queue_entries = 0;

uint32_t nvme_read32(uint32_t offset)
{
    return mmio_readl(nvme_virt_base + offset / 4);
}
uint64_t nvme_read64(uint32_t offset)
{
    auto low = mmio_readl(nvme_virt_base + offset / 4);
    auto high = mmio_readl(nvme_virt_base + offset / 4 + 1);
    return (uint64_t)high << 32 | low;
}
void nvme_write32(uint32_t offset, uint32_t value)
{
    mmio_writel(nvme_virt_base + offset / 4, value);
}

pmos::async::task<void> kinda_busy_wait(uint64_t duration_ms, auto func)
{
    uint64_t waited_time = 0;
    while (duration_ms > waited_time) {
        if (func())
            co_return;

        co_await dispatcher.wait_for(10'000'000); // 10 ms
        waited_time += 10;
    }

    co_return;
}

pmos::async::task<void> wait_for_status(bool status)
{
    co_await kinda_busy_wait(timeout * 500, [status]() {
        auto csts = nvme_read32(NVME_REG_CSTS);
        return ((csts & 1) == status);
    });

    if ((nvme_read32(NVME_REG_CSTS) & 1) != status) {
        std::cout << "NVMe controller did not reach status " << status << " within timeout\n";
        exit(1);
    }

    co_return;
}

pmos::async::detached_task nvme_handle()
{
    nvme_controller = std::make_unique<pmos::DeviceHandler>(dispatcher, device_right);

    printf("NVMe controller created\n");

    // Enable DMA and memory space access
    uint32_t command = co_await nvme_controller->pci_readw(0x4);
    command |= 0x06;
    co_await nvme_controller->pci_writew(0x4, command);

    // Get BAR0
    auto resource = co_await nvme_controller->get_resource(DeviceHandler::PCIBar{}, 0, {}, DeviceHandler::ResourceType::Memory);
    uint64_t bar = resource.base;
    uint64_t size = resource.size;
    printf("NVMe MMIO: %" PRIx64 ", size: %" PRIx64 "\n", bar, size);

    auto page_size = getpagesize();
    size = std::max<uint64_t>(size, page_size);

    auto mem_req = create_phys_map_region(0, nullptr, size, PROT_READ | PROT_WRITE, bar);
    if (mem_req.result != SUCCESS) {
        printf("Failed to map AHCI controller's memory\n");
        exit(1);
    }
    nvme_virt_base = reinterpret_cast<uint32_t *>(mem_req.virt_addr);

    // Attach PCI interrupt
    auto r = co_await nvme_controller->register_interrupt();
    handle_interrupts(std::move(r));

    printf("Interrupt registered...\n");

    auto cap = nvme_read64(NVME_REG_CAP);
    auto vs = nvme_read32(NVME_REG_VS);
    printf("NVMe version: %x.%x\n", (vs >> 16) & 0xffff, vs & 0xffff);
    printf("NVMe CAP: %#" PRIx64 "\n", cap);

    if (!(cap & ((uint64_t)1 << 37))) {
        std::cout << "The controller does not support NVM command set\n";
        exit(1);
    }

    uint32_t mpsmin = (cap >> 48) & 0xf;
    uint32_t mpsmax = (cap >> 52) & 0xf;
    doorbell_stride = (cap >> 32) & 0xf;
    timeout = (cap >> 24) & 0xff;
    maximum_queue_entries = (cap & 0xffff) + 1;

    // NVMe's page size is +12
    uint32_t page_order_normal = std::countr_zero((uint32_t)page_size) - 12;
    if ((page_order_normal < mpsmin) || (page_order_normal > mpsmax)) {
        std::cout << "NVMe controller does not support page size of " << page_size << " bytes (mpsmin: "
                  << mpsmin << ", mpsmax: " << mpsmax << ")\n";
        exit(1);
    }

    // Reset the controller
    nvme_write32(NVME_REG_CC, 0);
    co_await wait_for_status(false);

    // Enable the controller
    uint32_t cc = (page_order_normal << 7) | 1;
    nvme_write32(NVME_REG_CC, cc);
    co_await wait_for_status(true);

    std::cout << "Enabled NVMe controller..." << std::endl;
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
    printf("NVMe started...\n");
    parse_args(argc, argv);

    // init_timer();
    nvme_handle();
    // nvme_controller_main();

    dispatcher.dispatch().value();

    printf("!!! nvme: Exiting... !!!\n");
}
