#include "namespace.hh"
#include "common.hh"
#include "queue.hh"
#include "spec.hh"
#include <iostream>
#include <pmos/device_handler.hh>
#include <unistd.h>
#include <pmos/utility/scope_guard.hh>
#include <pmos/memory.h>
#include <sys/mman.h>
#include <memory>

pmos::async::detached_task handle_namespace(uint32_t namespace_id)
{
    std::cout << "Identifying namespace " << namespace_id << "...\n";

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
    cmd.nsid = namespace_id;
    cmd.prp1 = phys_addr.phys_addr;
    cmd.cdw10 = 0; // Identify namespace

    pmos::dma_wmb();

    auto completion_entry = co_await admin_submission_queue->submit_command(cmd);
    if (!completion_entry.success()) {
        std::cout << "Identify controller command failed with status: " << completion_entry.status << "\n";
        exit(1);
    }

    pmos::dma_rmb();

    auto *ns = reinterpret_cast<IdentifyNamespace *>(region.virt_addr);
    auto flbas = ns->flbas & 0xf;
    auto lbaf = ns->lbaf[flbas];
    unsigned block_size = 1u << lbaf.ds;
    auto size = ns->nsze;

    std::cout << "Namespace " << namespace_id << ": size (blocks) = " << size << ", block size = " << block_size << "\n";

    auto ns_ptr = std::make_shared<NVMeNamespace>();
    ns_ptr->id = namespace_id;
    ns_ptr->size = size;
    ns_ptr->block_size = block_size;
}