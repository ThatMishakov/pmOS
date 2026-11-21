#include "queue.hh"
#include "spec.hh"
#include "namespace.hh"

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
#include <pmos/utility/scope_guard.hh>
#include <pmos/containers/intrusive_list.hh>

using namespace pmos;

pmos::Right device_right;

pmos::Port cmd_port = pmos::Port::create().value();
auto dispatcher = pmos::PortDispatcher(cmd_port);
auto pmbus_helper = pmos::PMBUSHelper(dispatcher);

std::unique_ptr<pmos::DeviceHandler> nvme_controller = nullptr;

uint32_t *nvme_virt_base = nullptr;

uint32_t doorbell_stride = 0; // register * (1 << doorbell_stride)
uint32_t timeout = 0; // 500 milliseconds units
uint32_t maximum_queue_entries = 0;

unsigned rab = 0;
unsigned mdts = 0;
unsigned nn = 0;

uint32_t vs = 0;

std::string serial_number;

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

std::shared_ptr<NVMeCompletionQueue> admin_completion_queue;
std::shared_ptr<NVMeSubmissionQueue> admin_submission_queue;

std::shared_ptr<NVMeCompletionQueue> data_completion_queue;
std::shared_ptr<NVMeSubmissionQueue> data_submission_queue;

void init_admin_queue()
{
    auto page_size = getpagesize();
    auto request = create_normal_region(0, nullptr, page_size, PROT_READ | PROT_WRITE | CREATE_FLAG_DMA);
    if ((long)request.result < 0) {
        std::cout << "Failed to allocate memory for NVMe admin completion queue: " << (int)request.result << " (" << strerror(-request.result) << ")\n";
        exit(1);
    }
    admin_completion_queue = std::make_shared<NVMeCompletionQueue>();
    admin_completion_queue->dma_virt_base = request.virt_addr;

    request = create_normal_region(0, nullptr, page_size, PROT_READ | PROT_WRITE | CREATE_FLAG_DMA);
    if ((long)request.result < 0) {
        std::cout << "Failed to allocate memory for NVMe admin submission queue: " << (int)request.result << " (" << strerror(-request.result) << ")\n";
        exit(1);
    }
    admin_submission_queue = std::make_shared<NVMeSubmissionQueue>();
    admin_submission_queue->dma_virt_base = request.virt_addr;
    admin_submission_queue->completion_queue = admin_completion_queue;
    admin_completion_queue->submission_queue = admin_submission_queue.get();

    constexpr uint32_t maximum_admin_queue_entries = 4096;

    admin_completion_queue->size = std::min<uint32_t>(maximum_admin_queue_entries, page_size / COMPLETION_QUEUE_ENTRY_SIZE);
    admin_submission_queue->size = std::min<uint32_t>(maximum_admin_queue_entries, page_size / SUBMISSION_QUEUE_ENTRY_SIZE);

    auto phys_addr = get_page_phys_address(0, admin_completion_queue->dma_virt_base, 0);
    if ((long)phys_addr.result < 0) {
        std::cout << "Could not get physical address for NVMe admin completion queue: " << (int)phys_addr.result << " (" << strerror(-phys_addr.result) << ")\n";
        exit(1);
    }
    nvme_write64(NVME_REG_ACQ, phys_addr.phys_addr);
    phys_addr = get_page_phys_address(0, admin_submission_queue->dma_virt_base, 0);
    if ((long)phys_addr.result < 0) {
        std::cout << "Could not get physical address for NVMe admin submission queue: "<< (int)phys_addr.result << " (" << strerror(-phys_addr.result) << ")\n";
        exit(1);
    }
    nvme_write64(NVME_REG_ASQ, phys_addr.phys_addr);

    uint32_t aqa = ((admin_completion_queue->size - 1) << 16) | (admin_submission_queue->size - 1);
    nvme_write32(NVME_REG_AQA, aqa);

    // Enable the interrupt
    nvme_write32(NVME_REG_INTMC, 1);
}

pmos::async::task<void> create_data_queues()
{
    auto page_size = getpagesize();
    auto request = create_normal_region(0, nullptr, page_size, PROT_READ | PROT_WRITE | CREATE_FLAG_DMA);
    if ((long)request.result < 0) {
        std::cout << "Failed to allocate memory for NVMe data completion queue: " << (int)request.result << " (" << strerror(-request.result) << ")\n";
        exit(1);
    }
    data_completion_queue = std::make_shared<NVMeCompletionQueue>();
    data_completion_queue->dma_virt_base = request.virt_addr;

    request = create_normal_region(0, nullptr, page_size, PROT_READ | PROT_WRITE | CREATE_FLAG_DMA);
    if ((long)request.result < 0) {
        std::cout << "Failed to allocate memory for NVMe data submission queue: " << (int)request.result << " (" << strerror(-request.result) << ")\n";
        exit(1);
    }
    data_submission_queue = std::make_shared<NVMeSubmissionQueue>();
    data_submission_queue->dma_virt_base = request.virt_addr;
    data_submission_queue->completion_queue = data_completion_queue;
    data_completion_queue->submission_queue = data_submission_queue.get();

    constexpr uint32_t maximum_data_queue_entries = 4096;

    data_completion_queue->size = std::min<uint32_t>(maximum_data_queue_entries, page_size / COMPLETION_QUEUE_ENTRY_SIZE);
    data_submission_queue->size = std::min<uint32_t>(maximum_data_queue_entries, page_size / SUBMISSION_QUEUE_ENTRY_SIZE);

    uint32_t queue_id = 1;

    auto phys_addr = get_page_phys_address(0, data_completion_queue->dma_virt_base, 0);
    if ((long)phys_addr.result < 0) {
        std::cout << "Could not get physical address for NVMe data completion queue: " << (int)phys_addr.result << " (" << strerror(-phys_addr.result) << ")\n";
        exit(1);
    }

    NVMeCommand cmd = {};
    cmd.opcode = 0x05; // Create I/O Completion Queue
    cmd.command_id = 2;
    cmd.prp1 = phys_addr.phys_addr;
    cmd.cdw10 = queue_id | ((data_completion_queue->size - 1) << 16);
    cmd.cdw11 = (1 << 0) | (1 << 1);

    pmos::dma_wmb();

    auto completion_entry = co_await admin_submission_queue->submit_command(cmd);
    if (!completion_entry.success()) {
        std::cout << "Create I/O Completion Queue command failed with status: " << completion_entry.status << "\n";
        exit(1);
    }

    phys_addr = get_page_phys_address(0, data_submission_queue->dma_virt_base, 0);
    if ((long)phys_addr.result < 0) {
        std::cout << "Could not get physical address for NVMe data submission queue: " << (int)phys_addr.result << " (" << strerror(-phys_addr.result) << ")\n";
        exit(1);
    }

    cmd = {};
    cmd.opcode = 0x01; // Create I/O Submission Queue
    cmd.command_id = 3;
    cmd.prp1 = phys_addr.phys_addr;
    cmd.cdw10 = queue_id | ((data_submission_queue->size - 1) << 16);
    cmd.cdw11 = (queue_id << 16) | (1 << 0);

    pmos::dma_wmb();

    completion_entry = co_await admin_submission_queue->submit_command(cmd);
    if (!completion_entry.success()) {
        std::cout << "Create I/O Submission Queue command failed with status: " << completion_entry.status << "\n";
        exit(1);
    }
}

pmos::async::task<void> identify_controller()
{
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
    cmd.prp1 = phys_addr.phys_addr;
    cmd.cdw10 = 1; // Identify controller

    pmos::dma_wmb();

    auto completion_entry = co_await admin_submission_queue->submit_command(cmd);
    if (!completion_entry.success()) {
        std::cout << "Identify controller command failed with status: " << completion_entry.status << "\n";
        exit(1);
    }

    pmos::dma_rmb();


    auto *controller = reinterpret_cast<IdentifyController *>(region.virt_addr);

    serial_number = std::string(controller->sn, sizeof(controller->sn));

    std::cout << "Controller ID: " << controller->cntlid << "\n";
    std::cout << "Vendor ID: " << controller->vid << "\n";
    std::cout << "Subsystem Vendor ID: " << controller->ssvid << "\n";
    std::cout << "Serial Number: " << serial_number << "\n";
    std::cout << "Model Number: " << std::string(controller->mn, sizeof(controller->mn)) << "\n";
    std::cout << "Firmware Revision: " << std::string(controller->fr, sizeof(controller->fr)) << "\n";

    rab = controller->rab;
    mdts = controller->mdts;
    nn = controller->nn;
}

pmos::async::detached_task identify_namespaces()
{
    std::cout << "Identifying namespaces...\n";
    
    if (vs >= 0x00010100U) {
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
        cmd.prp1 = phys_addr.phys_addr;
        cmd.cdw10 = 2; // Identify namespaces

        pmos::dma_wmb();

        auto completion_entry = co_await admin_submission_queue->submit_command(cmd);
        if (!completion_entry.success()) {
            std::cout << "Identify namespaces command failed with status: " << completion_entry.status << "\n";
            exit(1);
        }

        pmos::dma_rmb();

        auto *namespaces = reinterpret_cast<uint32_t *>(region.virt_addr);
        for (unsigned i = 0; i < nn; ++i) {
            if (namespaces[i] == 0)
                break;

            handle_namespace(namespaces[i]);
        }
    } else {
        for (unsigned i = 1; i <= nn; ++i) {
            handle_namespace(i);
        }
    }
}

void check_nvme_interrupts()
{
    // TODO: Do this for other queues...
    assert(admin_completion_queue);
    admin_completion_queue->check_for_completions();

    if (data_completion_queue)
        data_completion_queue->check_for_completions();
}

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
            // Mask all interrupts, then complete them, then handle them
            nvme_write32(NVME_REG_INTMS, 0xffffffff);
            pmos::complete_interrupt(int_right);

            check_nvme_interrupts();
        } break;
        default:
            printf("NVMe: unknown message type: %i in interrupt handler!\n", request->type);
            exit(1);
            break;
        }
    }
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

    auto cap = nvme_read64(NVME_REG_CAP);
    vs = nvme_read32(NVME_REG_VS);
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

    // Mask interrupts
    nvme_write32(NVME_REG_INTMS, 0xffffffff);

    init_admin_queue();

    // Enable the controller
    uint32_t cc = (6u << 16) | (4u << 20) | (page_order_normal << 7) | 1u;
    nvme_write32(NVME_REG_CC, cc);
    co_await wait_for_status(true);

    std::cout << "Enabled NVMe controller..." << std::endl;

    // Attach PCI interrupt
    auto r = co_await nvme_controller->register_interrupt();
    handle_interrupts(std::move(r));

    printf("Interrupt registered...\n");

    co_await identify_controller();
    co_await create_data_queues();

    identify_namespaces();
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
