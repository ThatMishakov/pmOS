/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "limine.h"

#include "../memory/paging.hh"
#include "../memory/pmm.hh"
#include "../memory/temp_mapper.hh"
#include "../memory/vmm.hh"

#include <backends/fb.h>
#include <dtb/dtb.hh>
#include <flanterm.h>
#include <kern_logger/kern_logger.hh>
#include <memory/mem_object.hh>
#include <paging/arch_paging.hh>
#include <processes/tasks.hh>
#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <uacpi/uacpi.h>
#include <uacpi/kernel_api.h>
#include "kernel_pages.hh"
#include "common.hh"

// Nice code!
#if defined(__x86_64__) || defined(__riscv) || defined(__loongarch64)

using namespace kernel;
using namespace kernel::pmm;
using namespace kernel::log;
using namespace kernel::paging;
using namespace kernel::proc;

extern "C" void limine_main();
extern "C" void _limine_entry();

u64 kernel_phys_base = 0;

static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(5);

__attribute__((used)) limine_bootloader_info_request boot_request = {
        .id       = LIMINE_BOOTLOADER_INFO_REQUEST_ID,
        .revision = 0,
        .response = nullptr,
};

__attribute__((used)) limine_memmap_request memory_request = {
    .id       = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

__attribute__((used)) limine_entry_point_request entry_point_request = {
    .id       = LIMINE_ENTRY_POINT_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
    .entry    = _limine_entry,
};

__attribute__((used)) limine_executable_address_request kernel_address_request = {
    .id       = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

__attribute__((used)) limine_hhdm_request hhdm_request = {
    .id       = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

__attribute__((used)) limine_paging_mode_request paging_request = {
    .id       = LIMINE_PAGING_MODE_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
    #ifdef __riscv
    .mode = LIMINE_PAGING_MODE_RISCV_SV57,
    #else
    .mode = 0,
    #endif
};

extern void hcf();

Direct_Mapper init_mapper;

    // Temporary temporary mapper
    #ifdef __x86_64__
        #include <paging/x86_temp_mapper.hh>
        #include <x86_asm.hh>
using Arch_Temp_Mapper = x86_64::paging::x86_PAE_Temp_Mapper;
Arch_Temp_Mapper temp_temp_mapper;

    #elif defined(__riscv)
        #include <paging/riscv64_temp_mapper.hh>
using Arch_Temp_Mapper = kernel::riscv64::paging::RISCV64_Temp_Mapper;
Arch_Temp_Mapper temp_temp_mapper;

    #elif defined(__loongarch64)
        #include <paging/loong_temp_mapper.hh>

    #endif

u64 hhdm_offset = 0;

namespace kernel::sched
{
extern size_t number_of_cpus;
}

extern void *_kernel_start;

extern void *_text_start;
extern void *_text_end;

extern void *_rodata_start;
extern void *_rodata_end;

extern void *_data_start;
extern void *_data_end;

extern void *_bss_start;
extern void *_bss_end;

extern void *__eh_frame_start;
extern void *__eh_frame_end;
extern void *_gcc_except_table_start;
extern void *_gcc_except_table_end;
extern u8 _kernel_end;

ptable_top_ptr_t kernel_ptable_top = 0;

static klib::vector<limine_memmap_entry> *limine_memory_regions = nullptr;

    #ifdef __x86_64__
extern ulong idle_cr3;
    #elif defined(__riscv)
namespace kernel::riscv64::paging
{
extern ulong idle_pt;
}
    #endif

extern phys_addr_t temp_alloc_base;
extern phys_addr_t temp_alloc_size;
extern phys_addr_t temp_alloc_reserved;
extern long temp_alloc_entry_id;
    
flanterm_context *ft_ctx = nullptr;
uint32_t *fb_virt        = nullptr;

__attribute__((used)) struct limine_framebuffer_request fb_req = {
    .id       = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = nullptr,
};

void init_fb()
{
    struct limine_framebuffer_response r;
    copy_from_phys((u64)fb_req.response - hhdm_offset, &r, sizeof(r));

    limine_framebuffer *framebuffers[r.framebuffer_count];
    copy_from_phys((u64)r.framebuffers - hhdm_offset, framebuffers,
                   r.framebuffer_count * sizeof(limine_framebuffer *));

    if (r.framebuffer_count != 0) {
        limine_framebuffer fb;
        copy_from_phys((u64)framebuffers[0] - hhdm_offset, &fb, sizeof(fb));

        size_t fb_size   = fb.pitch * fb.height;
        size_t fb_offset = size_t(fb_size) & 0xFFF;
        fb_size          = (fb_size + fb_offset + 0xFFF) & ~0xFFF;
        fb_virt          = (uint32_t *)vmm::kernel_space_allocator.virtmem_alloc(fb_size / PAGE_SIZE);
        if (!fb_virt)
            return;

        Page_Table_Argumments args = {
            .readable           = true,
            .writeable          = true,
            .user_access        = false,
            .global             = true,
            .execution_disabled = true,
            .extra              = 0,
        };

        uint64_t phys_addr_all = (uint64_t)fb.address - hhdm_offset - fb_offset;
        auto result =
            map_kernel_pages(phys_addr_all, fb_virt, fb_size, args);
        if (result != 0) {
            serial_logger.printf("Failed to map framebuffer\n");
            vmm::kernel_space_allocator.virtmem_free((void *)fb_virt, fb_size / PAGE_SIZE);
            return;
        }

        ft_ctx = flanterm_fb_init(
            malloc, [](void *ptr, size_t) { free(ptr); }, fb_virt, fb.width, fb.height,
            fb.pitch, fb.red_mask_size, fb.red_mask_shift, fb.green_mask_size, fb.green_mask_shift,
            fb.blue_mask_size, fb.blue_mask_shift, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            0, 0, 1, 0, 0, 0);
    }
}

void construct_paging()
{
    serial_logger.printf("Finding the largest memory region...\n");
    limine_memmap_response *r = memory_request.response;
    if (r == nullptr) {
        hcf();
    }
    limine_memmap_response resp = *r;

    hhdm_offset = hhdm_request.response->offset;

    for (uint64_t i = 0; i < resp.entry_count; i++) {
        uint64_t type = resp.entries[i]->type;
        if (type == LIMINE_MEMMAP_USABLE) {
            if (temp_alloc_size < resp.entries[i]->length) {
                temp_alloc_base     = resp.entries[i]->base;
                temp_alloc_size     = resp.entries[i]->length;
                temp_alloc_entry_id = i;
            }
        }

        // Print memory map
        serial_logger.printf(" Memory map region %li: 0x%lh - 0x%lh, type %li\n", i,
                             resp.entries[i]->base, resp.entries[i]->base + resp.entries[i]->length,
                             resp.entries[i]->type);
    }

    serial_logger.printf("Found the largest memory region at 0x%lh, size 0x%lh\n", temp_alloc_base,
                         temp_alloc_size);

    init_mapper.virt_offset = hhdm_request.response->offset;
    global_temp_mapper      = &init_mapper;

    serial_logger.printf("Initializing paging...\n");

    #ifdef __riscv
    auto rr                                        = paging_request.response;
    kernel::riscv64::paging::riscv64_paging_levels = rr->mode + 3;
    serial_logger.printf("Using %i paging levels\n",
                         kernel::riscv64::paging::riscv64_paging_levels);
    #endif

    // While we're here, initialize virtmem
    #ifdef __riscv
    const u64 heap_space_shift = 12 + (kernel::riscv64::paging::riscv64_paging_levels - 1) * 9;
    #else
    const u64 heap_space_shift = 12 + 27;
    #endif

    const u64 heap_space_start = (-1UL) << (heap_space_shift + 8);
    const u64 heap_addr_size   = -heap_space_start;

    const size_t PAGE_MASK = PAGE_SIZE - 1;
    void *kernel_start     = (void *)(((size_t)&_kernel_start + PAGE_MASK) & ~PAGE_MASK);
    const size_t kernel_size =
        ((char *)&_kernel_end - (char *)kernel_start + PAGE_SIZE - 1) & ~PAGE_MASK;
    vmm::virtmem_init((void *)heap_space_start, heap_addr_size, kernel_start, kernel_size);

    kernel_ptable_top = pmm::get_memory_for_kernel(1);
    clear_page(kernel_ptable_top);
    #ifdef __x86_64__
    idle_cr3 = kernel_ptable_top;
    #elif defined(__riscv)
    riscv64::paging::idle_pt = kernel_ptable_top;
    #endif

    #ifndef __loongarch64
    // Init temp mapper with direct map, while it is still available
    void *temp_mapper_start = vmm::kernel_space_allocator.virtmem_alloc_aligned(
        16, 4); // 16 pages aligned to 16 pages boundary
    temp_temp_mapper = Arch_Temp_Mapper(temp_mapper_start, kernel_ptable_top);
    #endif

    kernel_phys_base = kernel_address_request.response->physical_base;
    map_kernel_pages(kernel_ptable_top, kernel_phys_base);

    serial_logger.printf("Switching to in-kernel page table...\n");

    apply_page_table(kernel_ptable_top);

    // Set up the right mapper (since there is no direct map anymore) and bitmap
    #ifndef __loongarch64
    global_temp_mapper = &temp_temp_mapper;
    #else
    loongarch64::paging::set_dmws();
    global_temp_mapper = &loongarch64::paging::temp_mapper;
    #endif

    serial_logger.printf("Paging initialized!\n");

    init_fb();

    klib::vector<limine_memmap_entry *> regions;
    if (!regions.resize(resp.entry_count))
        panic("Failed to reserve memory for regions");

    serial_logger.printf("Copying memory regions...\n");

    copy_from_phys((u64)resp.entries - hhdm_offset, regions.data(),
                   resp.entry_count * sizeof(limine_memmap_entry *));

    limine_memory_regions = new klib::vector<limine_memmap_entry> {};
    if (!limine_memory_regions)
        panic("Failed to allocate memory for memory_regions");

    klib::vector<limine_memmap_entry> &regions_data = *limine_memory_regions;
    if (!regions_data.resize(resp.entry_count))
        panic("Failed to reserve memory for regions_data");

    for (size_t i = 0; i < resp.entry_count; i++) {
        copy_from_phys((u64)regions[i] - hhdm_offset, &regions_data[i],
                       sizeof(limine_memmap_entry));
    }

    if (!memory_map.reserve(regions.size()))
        panic("Could not allocate memory for memory map");

    for (auto region: regions_data) {
        MemoryRegionType type;
        switch (region.type) {
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
        case LIMINE_MEMMAP_RESERVED_MAPPED:
            type = MemoryRegionType::UsableReservedOnBoot;
            break;
        case LIMINE_MEMMAP_USABLE:
            type = MemoryRegionType::Usable;
            break;
        case LIMINE_MEMMAP_RESERVED:
            type = MemoryRegionType::Reserved;
            break;
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            type = MemoryRegionType::ACPIReclaimable;
            break;
        case LIMINE_MEMMAP_ACPI_NVS:
            type = MemoryRegionType::ACPINVS;
            break;
        case LIMINE_MEMMAP_BAD_MEMORY:
            type = MemoryRegionType::BadMemory;
            break;
        case LIMINE_MEMMAP_FRAMEBUFFER:
            type = MemoryRegionType::Framebuffer;
            break;
        default:
            type = MemoryRegionType::Unknown;
            break;
        }

        memory_map.push_back({
            .start = region.base,
            .size  = region.length,
            .type  = type,
        });
    }

    init_pmm(kernel_ptable_top);
}

__attribute__((used)) limine_module_request module_request = {
    .id                    = LIMINE_MODULE_REQUEST_ID,
    .revision              = 1,
    .response              = nullptr,
    .internal_module_count = 0,
    .internal_modules      = nullptr,
};

void init_modules()
{
    limine_module_response *resp = module_request.response;
    if (resp == nullptr)
        return;

    const u64 resp_phys = (u64)resp - hhdm_offset;
    limine_module_response r;
    copy_from_phys(resp_phys, &r, sizeof(r));

    klib::vector<limine_file *> modules;
    if (!modules.resize(r.module_count))
        panic("Failed to reserve memory for modules");

    const u64 files_phys = (u64)r.modules - hhdm_offset;
    copy_from_phys(files_phys, modules.data(), r.module_count * sizeof(limine_file *));

    if (!::modules.reserve(r.module_count))
        panic("Failed to reserve memory for modules");

    for (auto &mm: modules) {
        limine_file f;
        copy_from_phys((u64)mm - hhdm_offset, &f, sizeof(f));
        module m = {
            .path      = capture_from_phys((u64)f.path - hhdm_offset),
            .cmdline   = capture_from_phys((u64)f.string - hhdm_offset),
            .phys_addr = (u64)f.address - hhdm_offset,
            .size      = f.size,
            .object    = Mem_Object::create_from_phys((u64)f.address - hhdm_offset,
                                                      (f.size + 0xfff) & ~0xfffUL, true),
        };

        serial_logger.printf("Module: %s, cmdline: %s\n", m.path.c_str(), m.cmdline.c_str());
        ::modules.push_back(m);
    }
}

__attribute__((used)) struct limine_framebuffer_request fb_req = {
    .id       = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

klib::vector<klib::unique_ptr<load_tag_generic>> construct_load_tag_framebuffer()
{
    klib::vector<klib::unique_ptr<load_tag_generic>> tags {};

    if (fb_req.response == nullptr)
        return tags;

    struct limine_framebuffer_response r;
    copy_from_phys((u64)fb_req.response - hhdm_offset, &r, sizeof(r));

    limine_framebuffer *framebuffers[r.framebuffer_count];
    copy_from_phys((u64)r.framebuffers - hhdm_offset, framebuffers,
                   r.framebuffer_count * sizeof(limine_framebuffer *));

    if (!tags.reserve(r.framebuffer_count))
        panic("Failed to reserve memory for tags");

    for (size_t i = 0; i < r.framebuffer_count; i++) {
        limine_framebuffer fb;
        copy_from_phys((u64)framebuffers[i] - hhdm_offset, &fb, sizeof(fb));

        klib::unique_ptr<load_tag_generic> tag = (load_tag_generic *)new load_tag_framebuffer;
        if (!tag) {
            panic("Failed to allocate memory for framebuffer tag");
            tags.clear();
            return tags;
        }
        tag->tag            = LOAD_TAG_FRAMEBUFFER;
        tag->flags          = 0;
        tag->offset_to_next = sizeof(load_tag_framebuffer);

        load_tag_framebuffer *desc = (load_tag_framebuffer *)tag.get();
        desc->framebuffer_addr     = (u64)fb.address - hhdm_offset;
        desc->framebuffer_pitch    = fb.pitch;
        desc->framebuffer_width    = fb.width;
        desc->framebuffer_height   = fb.height;
        desc->framebuffer_bpp      = fb.bpp;
        desc->memory_model         = fb.memory_model;
        desc->red_mask_size        = fb.red_mask_size;
        desc->red_mask_shift       = fb.red_mask_shift;
        desc->green_mask_size      = fb.green_mask_size;
        desc->green_mask_shift     = fb.green_mask_shift;
        desc->blue_mask_size       = fb.blue_mask_size;
        desc->blue_mask_shift      = fb.blue_mask_shift;
        desc->unused[0]            = 0;

        tags.push_back(klib::move(tag));
    }

    return tags;
}

klib::unique_ptr<load_tag_generic> construct_load_tag_rsdp()
{
    if (rsdp == RSDP_INITIALIZER)
        return {};

    klib::unique_ptr<load_tag_generic> tag = (load_tag_generic *)new load_tag_rsdp;

    auto *t   = (load_tag_rsdp *)tag.get();
    t->header = LOAD_TAG_RSDP_HEADER;
    t->rsdp   = rsdp;

    return tag;
}

klib::unique_ptr<load_tag_generic> construct_load_tag_fdt()
{
    if (not dtb_object)
        return nullptr;

    klib::unique_ptr<load_tag_generic> tag = (load_tag_generic *)new load_tag_fdt;

    auto t               = (load_tag_fdt *)tag.get();
    t->header            = LOAD_TAG_FDT_HEADER;
    t->fdt_memory_object = dtb_object->get_id();
    t->start_offset      = 0; // TODO?
    t->mem_object_size   = dtb_object->size_bytes();

    return tag;
}

namespace kernel::paging
{
extern klib::shared_ptr<Arch_Page_Table> idle_page_table;
}

void init(void);
void init_scheduling(u64 boot_cpu_id);
klib::vector<u64> initialize_cpus(const klib::vector<u64> &hartids);
void *get_cpu_start_func();

void init_task1()
{
    // Find task 1 module.
    // For now, just search for "bootstrap"
    module *task1                = nullptr;
    const klib::string bootstrap = "bootstrap";
    for (auto &m: modules) {
        serial_logger.printf("Module %s\n", m.cmdline.c_str());
        if (m.cmdline == bootstrap) {
            task1 = &m;
            break;
        }
    }

    if (task1 == nullptr) {
        serial_logger.printf("Task 1 not found\n");
        hcf();
    }

    serial_logger.printf("Task 1 found: %s\n", task1->path.c_str());

    // Create new task and load ELF into it
    auto task = TaskDescriptor::create_process(TaskDescriptor::PrivilegeLevel::User);
    if (!task)
        panic("Failed to create task");
    task->name = "bootstrap";
    serial_logger.printf("Loading ELF...\n");

    auto group = proc::TaskGroup::create_for_task(task);
    if (!group)
        panic("Failed to create task group for task 1");

    auto object = ipc::MemObjectRight::create_for_group(task1->object, group.val, ipc::MemObjectRight::PERM_READ);
    if (!object)
        panic("Failed to create memory object right for task 1");

    // Pass the modules to the task
    klib::vector<klib::unique_ptr<load_tag_generic>> tags;
    tags = construct_load_tag_framebuffer();

    auto t = construct_load_tag_rsdp();
    if (t && !tags.push_back(klib::move(t)))
        panic("Failed to add RSDP tag");

    t = construct_load_tag_fdt();
    if (t && !tags.push_back(klib::move(t)))
        panic("Failed to add FDT tag");

    t = construct_load_tag_for_modules(group.val);
    if (t && !tags.push_back(klib::move(t)))
        panic("Failed to add modules tag");

    auto p = task->atomic_load_elf(object.val, task1->path, tags, group.val);
    if (!p.success() || !p.val)
        panic("Failed to load task 1: %i", p.result);
}

__attribute__((used)) limine_rsdp_request rsdp_request = {
    .id       = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

void init_acpi()
{
    if (rsdp_request.response == nullptr) {
        serial_logger.printf("No RSDP found\n");
        return;
    }

    limine_rsdp_response resp;
    copy_from_phys((u64)rsdp_request.response - hhdm_offset, &resp, sizeof(resp));

    if (resp.address == 0) {
        serial_logger.printf("RSDP not found\n");
        return;
    }

    u64 addr = (u64)resp.address - hhdm_offset;
    serial_logger.printf("RSDP found at 0x%x\n", addr);
   
    init_acpi(addr);
}

__attribute__((used)) limine_dtb_request dtb_request = {
    .id       = LIMINE_DTB_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

__attribute__((used)) limine_riscv_bsp_hartid_request hartid_request = {
    .id = LIMINE_RISCV_BSP_HARTID_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

void init_dtb()
{
    if (dtb_request.response == nullptr) {
        return;
    }

    limine_dtb_response resp;
    copy_from_phys((u64)dtb_request.response - hhdm_offset, &resp, sizeof(resp));

    u64 addr = (u64)resp.dtb_ptr - hhdm_offset;
    serial_logger.printf("DTB found at 0x%x\n", addr);

    init_dtb((u64)addr);
}

u64 bsp_cpu_id = 0;

namespace kernel::sched
{
extern bool other_cpus_online;
}

size_t booted_cpus      = 0;
bool boot_barrier_start = false;

    #if defined(__x86_64__)
extern u64 boot_tsc;
    #endif

void init_smp();
void init_scheduling_on_bsp();

void limine_main()
{
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

#ifdef __x86_64__
    boot_tsc = rdtsc();
#endif

    serial_logger.printf("Hello from pmOS kernel!\n");
    serial_logger.printf("Kernel start: 0x%lh\n", &_kernel_start);

    #ifdef __riscv
    if (hartid_request.response)
        bsp_cpu_id = hartid_request.response->bsp_hartid;
    #endif

    construct_paging();

    serial_logger.printf("Calling global constructors...\n");

    // Call global (C++) constructors
    init();

    init_acpi();
    init_dtb();

    // Init idle task page table
    #ifndef __loongarch__
    idle_page_table = Arch_Page_Table::capture_initial(kernel_ptable_top);
    #else
    idle_page_table = Arch_Page_Table::create_empty();
    #endif
    if (!idle_page_table)
        panic("Could not create idle page table");

    #ifdef __x86_64__
    init_scheduling_on_bsp();
    #else
    init_scheduling(bsp_cpu_id);
    #endif

    // Switch to CPU-local temp mapper
    global_temp_mapper = nullptr;

    init_smp();

    init_modules();
    init_task1();
    serial_logger.printf("Loaded kernel...\n");

    __atomic_add_fetch(&booted_cpus, 1, __ATOMIC_RELAXED);

    // while (__atomic_load_n(&booted_cpus, __ATOMIC_ACQUIRE) < sched::number_of_cpus)
    //     // TODO: this is a potential deadlock with the new TLB shootdown mechanism
    //     ;
    // // All CPUs are booted

    serial_logger.printf("All CPUs booted. Cleaning up...\n");

    // Release bootloader-reserved memory
    for (auto r: *limine_memory_regions) {
        if (r.type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            serial_logger.printf("Releasing bootloader reclaimable 0x%h - 0x%h\n", r.base,
                                 r.base + r.length);
            kernel::pmm::free_memory_for_kernel(r.base, r.length / PAGE_SIZE);
        }
    }

    delete limine_memory_regions;

    bool tmp = true;
    __atomic_store(&boot_barrier_start, &tmp, __ATOMIC_RELEASE);

    serial_logger.printf("Bootstrap CPU entering userspace\n");
}

#endif