#include "limine.h"
#include "../memory/mem.hh"
#include "../memory/temp_mapper.hh"
#include "../memory/paging.hh"
#include "../memory/virtmem.hh"
#include <kern_logger/kern_logger.hh>

extern "C" void limine_main();
extern "C" void _limine_entry();

LIMINE_BASE_REVISION(1)

limine_bootloader_info_request boot_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0,
    .response = nullptr,
};

limine_memmap_request memory_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = nullptr,
};

limine_entry_point_request entry_point_request = {
    .id = LIMINE_ENTRY_POINT_REQUEST,
    .revision = 0,
    .response = nullptr,
    .entry = _limine_entry,
};

limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = nullptr,
};

limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = nullptr,
};

// Halt and catch fire function.
static void hcf(void) {
    // asm ("cli");
    // for (;;) {
    //     asm ("hlt");
    // }

    while (1) ;
}

inline void bitmap_set(uint64_t * bitmap, uint64_t bit) {
    bitmap[bit / 64] |= (1 << (bit % 64));
}

void bitmap_set_range(uint64_t * bitmap, uint64_t start, uint64_t end) {
    for (uint64_t i = start; i < end; i++) {
        bitmap_set(bitmap, i);
    }
}

inline void bitmap_clear(uint64_t * bitmap, uint64_t bit) {
    bitmap[bit / 64] &= ~(1 << (bit % 64));
}

void bitmap_clear_range(uint64_t * bitmap, uint64_t start, uint64_t end) {
    for (uint64_t i = start; i < end; i++) {
        bitmap_clear(bitmap, i);
    }
}

Direct_Mapper init_mapper;


// Temporary temporary mapper
#ifdef __x86_64__
using Arch_Temp_Mapper = x86_PAE_Temp_Mapper;
#elif defined(__riscv)
#include <paging/riscv64_temp_mapper.hh>
using Arch_Temp_Mapper = RISCV64_Temp_Mapper;
#endif

Arch_Temp_Mapper temp_temp_mapper;

u64 bitmap_phys = 0;
u64 bitmap_size_pages = 0;

void init_memory() {
    limine_memmap_response * resp = memory_request.response;
    if (resp == nullptr) {
        hcf();
    }

    serial_logger.printf("Initializing memory bitmap\n");

    uint64_t top_physical_address = 0;
    for (uint64_t i = 0; i < resp->entry_count; i++) {
        uint64_t type = resp->entries[i]->type;
        if (type == LIMINE_MEMMAP_USABLE ||
            type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            uint64_t end = resp->entries[i]->base + resp->entries[i]->length;
            if (end > top_physical_address) {
                top_physical_address = end;
            }
        }
    }
    // Align the top physical address to a page boundary
    // Not sure if it's necessary, but doesn't hurt
    top_physical_address = (top_physical_address + 0xfff) & ~0xfff;

    // Allocate physical memory for the free pages bitmap
    uint64_t bitmap_size = (top_physical_address / 0x1000) / 8;
    uint64_t bitmap_pages = (bitmap_size + 0xfff) / 0x1000;

    // Allocate the bitmap
    // Try and allocate the memory from the topmost free memory
    uint64_t bitmap_base_phys = 0;
    for (long long i = resp->entry_count - 1; i >= 0; i--) {
        if (resp->entries[i]->type == LIMINE_MEMMAP_USABLE &&
            resp->entries[i]->length >= bitmap_pages * 0x1000) {
            bitmap_base_phys = resp->entries[i]->base + resp->entries[i]->length - bitmap_pages * 0x1000;
            break;
        }
    }

    if (bitmap_base_phys == 0) {
        hcf();
    }

    uint64_t *bitmap_base_virt = (uint64_t*)(bitmap_base_phys + hhdm_request.response->offset);

    serial_logger.printf("Bitmap base: 0x%x, phys: 0x%x, bitmap size: 0x%x pages 0x%x top address 0x%x\n", bitmap_base_virt, bitmap_base_phys, bitmap_size, bitmap_pages, top_physical_address);

    // Init the bitmap
    uint64_t last_free_base = 0;
    for (uint64_t i = 0; i < resp->entry_count; i++) {
        serial_logger.printf("memmap entry %i: base: 0x%x, length: 0x%x, type: %i\n", i, resp->entries[i]->base, resp->entries[i]->length, resp->entries[i]->type);

        if (resp->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            uint64_t base = resp->entries[i]->base;
            uint64_t length = resp->entries[i]->length;

            // Everything between the last free base and the start of this entry is unusable
            uint64_t last_free_page = last_free_base / 0x1000;
            uint64_t this_free_page = base / 0x1000;
            uint64_t this_last_page = (base + length) / 0x1000;
            bitmap_clear_range(bitmap_base_virt, last_free_page, this_free_page);
            bitmap_set_range(bitmap_base_virt, this_free_page, this_last_page);
            last_free_base = base + length;
        }
    }
    {
        uint64_t last_free_page = last_free_base / 0x1000;
        uint64_t bitmap_pages_capacity = bitmap_pages * 0x1000 * 8;
        bitmap_clear_range(bitmap_base_virt, last_free_page, bitmap_pages_capacity);
    }

    // Reserve unusable regions and bitmap itself
    uint64_t bitmap_base_page = bitmap_base_phys / 0x1000;
    bitmap_clear_range(bitmap_base_virt, bitmap_base_page, bitmap_base_page + bitmap_pages);
    for (uint64_t i = 0; i < resp->entry_count; i++) {
        auto entry = resp->entries[i];
        if (entry->type == LIMINE_MEMMAP_RESERVED ||
            entry->type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
            entry->type == LIMINE_MEMMAP_ACPI_NVS ||
            entry->type == LIMINE_MEMMAP_BAD_MEMORY ||
            entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            uint64_t base = entry->base;
            uint64_t length = entry->length;
            uint64_t end_addr = base + length;
            if (end_addr > top_physical_address) {
                end_addr = top_physical_address;
            }
            uint64_t start_page = base / 0x1000;
            uint64_t end_page = (end_addr + 0xfff) / 0x1000;
            bitmap_clear_range(bitmap_base_virt, start_page, end_page);
        }
    }

    bitmap_phys = bitmap_base_phys;
    bitmap_size_pages = bitmap_pages;
    
    // Install the bitmap, as a physical address for now, since the kernel doesn't use its own paging yet
    kernel_pframe_allocator.init(bitmap_base_virt, bitmap_pages*0x1000/8);

    init_mapper.virt_offset = hhdm_request.response->offset;
    global_temp_mapper = &init_mapper;

    serial_logger.printf("Memory bitmap initialized. Top address: %u\n", top_physical_address);
}

extern void * _kernel_start;

extern void * _text_start;
extern void * _text_end;

extern void * _rodata_start;
extern void * _rodata_end;

extern void * _data_start;
extern void * _data_end;

extern void * _bss_start;
extern void * _bss_end;

extern void * __eh_frame_start;
extern void * __eh_frame_end;
extern void * _gcc_except_table_start;
extern void * _gcc_except_table_end;

const u64 kernel_space_start = (u64)(-1) >> 47 << 47;

void construct_paging() {
    serial_logger.printf("Initializing paging...\n");

    kresult_t result = SUCCESS;

    const u64 kernel_start_virt = (u64)&_kernel_start & ~0xfff;

    // While we're here, initialize virtmem
    virtmem_init(kernel_space_start, kernel_start_virt - kernel_space_start);

    ptable_top_ptr_t kernel_ptable_top = (u64)kernel_pframe_allocator.alloc_page();
    clear_page(kernel_ptable_top);

    // Init temp mapper with direct map, while it is still available
    void * temp_mapper_start = virtmem_alloc_aligned(16, 4); // 16 pages aligned to 16 pages boundary
    temp_temp_mapper = Arch_Temp_Mapper(temp_mapper_start, kernel_ptable_top);

    // Map bitmap
    void * bitmap_virt = virtmem_alloc(bitmap_size_pages);
    if (bitmap_virt == nullptr)
        hcf();
    const Page_Table_Argumments bitmap_args = {
        .writeable = true,
        .user_access = false,
        .global = true,
        .execution_disabled = true,
        .extra = 0,
    };
    map_pages(kernel_ptable_top, bitmap_phys, (u64)bitmap_virt, bitmap_size_pages*0x1000, bitmap_args);


    // Map kernel pages
    //
    // Prelude:
    // misha@Yoga:~/pmos/kernel$ readelf -l kernel

    // Elf file type is EXEC (Executable file)
    // Entry point 0xffffffff800200a8
    // There are 6 program headers, starting at offset 64

    // Program Headers:
    // Type           Offset             VirtAddr           PhysAddr
    //                 FileSiz            MemSiz              Flags  Align
    // RISCV_ATTRIBUT 0x000000000029d420 0x0000000000000000 0x0000000000000000
    //                 0x0000000000000048 0x0000000000000000  R      0x1
    // LOAD           0x0000000000001000 0xffffffff80000000 0xffffffff80000000
    //                 0x000000000002b728 0x000000000002b728  R E    0x1000
    // LOAD           0x000000000002c728 0xffffffff8002c728 0xffffffff8002c728
    //                 0x00000000000055f8 0x00000000000055f8  R      0x1000
    // LOAD           0x0000000000031d20 0xffffffff80032d20 0xffffffff80032d20
    //                 0x0000000000000878 0x0000000000007bf4  RW     0x1000
    // LOAD           0x0000000000032918 0xffffffff8003b918 0xffffffff8003b918
    //                 0x0000000000008c71 0x0000000000008c71  R      0x1000
    // DYNAMIC        0x0000000000000000 0x0000000000000000 0x0000000000000000
    //                 0x0000000000000000 0x0000000000000000  RW     0x8
    //...
    //  Section to Segment mapping:
    //    Segment Sections...
    //    00     .riscv.attributes
    //    01     .text
    //    02     .rodata .srodata .srodata._ZTS4Port ...
    //    03     .data .init_array ... .bss .sbss ...
    //    04     .eh_frame .gcc_except_table
    //
    // The kernel is loaded into higher half and has 4 memory regions:
    // 1. .text and related, with Read an Execute permissions
    // 2. .rodata, with Read permissions
    // 3. .data and .bss, with Read and Write permissions
    // 4. .eh_frame and .gcc_except_table, with Read only permissions
    // The addresses of these sections are known from the symbols, defined by linker script and their physical
    // location can be obtained from Kernel Address Feature Request by limine protocol
    // Map these pages and switch to kernel page table
    const u64 kernel_text_start = kernel_start_virt & ~0xfff;
    const u64 kernel_text_end = ((u64)&_text_end + 0xfff) & ~0xfff;
    
    const u64 kernel_phys = kernel_address_request.response->physical_base;
    const u64 text_phys = kernel_phys + kernel_text_start - kernel_start_virt;
    const u64 text_size = kernel_text_end - kernel_text_start;
    const u64 text_virt = text_phys - kernel_phys + kernel_start_virt;
    Page_Table_Argumments args = {
        .readable = true,
        .writeable = false,
        .user_access = false,
        .global = true,
        .execution_disabled = false,
        .extra = 0,
    };

    result = map_pages(kernel_ptable_top, text_phys, text_virt, text_size, args);
    if (result != SUCCESS)
        hcf();

    const u64 rodata_start = (u64)(&_rodata_start) & ~0xfff;
    const u64 rodata_end = ((u64)&_rodata_end + 0xfff) & ~0xfff;
    const u64 rodata_size = rodata_end - rodata_start;
    const u64 rodata_offset = rodata_start - kernel_start_virt;
    const u64 rodata_phys = kernel_phys + rodata_offset;
    const u64 rodata_virt = kernel_start_virt + rodata_offset;
    args = {true, false, false, true, true, 0};
    result = map_pages(kernel_ptable_top, rodata_phys, rodata_virt, rodata_size, args);
    if (result != SUCCESS)
        hcf();

    const u64 data_start = (u64)(&_data_start) & ~0xfff;
    // Data and BSS are merged and have the same permissions
    const u64 data_end = ((u64)&_bss_end + 0xfff) & ~0xfff;
    const u64 data_size = data_end - data_start;
    const u64 data_offset = data_start - kernel_start_virt;
    const u64 data_phys = kernel_phys + data_offset;
    const u64 data_virt = kernel_start_virt + data_offset;
    args = {true, true, false, true, true, 0};
    result = map_pages(kernel_ptable_top, data_phys, data_virt, data_size, args);
    if (result != SUCCESS)
        hcf();

    const u64 eh_frame_start = (u64)(&__eh_frame_start) & ~0xfff;
    // Same as with data; merge eh_frame and gcc_except_table
    const u64 eh_frame_end = ((u64)&_gcc_except_table_end + 0xfff) & ~0xfff;
    const u64 eh_frame_size = eh_frame_end - eh_frame_start;
    const u64 eh_frame_offset = eh_frame_start - kernel_start_virt;
    const u64 eh_frame_phys = kernel_phys + eh_frame_offset;
    const u64 eh_frame_virt = kernel_start_virt + eh_frame_offset;
    args = {true, false, false, true, true, 0};
    result = map_pages(kernel_ptable_top, eh_frame_phys, eh_frame_virt, eh_frame_size, args);
    if (result != SUCCESS)
        hcf();

    serial_logger.printf("Switching to in-kernel page table...\n");

    apply_page_table(kernel_ptable_top);

    // Set up the right mapper (since there is no direct map anymore) and bitmap
    global_temp_mapper = &temp_temp_mapper;
    kernel_pframe_allocator.init((u64 *)bitmap_virt, bitmap_size_pages*0x1000/8);
}

void init(void);

void limine_main() {
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    serial_logger.printf("Hello from pmOS kernel!\n");
   
    init_memory();
    construct_paging();

    // Call global (C++) constructors
    init();

    while (1) ;
}