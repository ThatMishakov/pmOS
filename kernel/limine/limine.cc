#include "limine.h"
#include "../memory/mem.hh"
#include "../memory/temp_mapper.hh"
#include "../memory/paging.hh"

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
    asm ("cli");
    for (;;) {
        asm ("hlt");
    }
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

void init_memory() {
    limine_memmap_response * resp = memory_request.response;
    if (resp == nullptr) {
        hcf();
    }

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

    // Init the bitmap
    uint64_t last_free_base = 0;
    for (uint64_t i = 0; i < resp->entry_count; i++) {
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

    // Install the bitmap, as a physical address for now, since the kernel doesn't use its own paging yet
    kernel_pframe_allocator.init(bitmap_base_virt, bitmap_pages*0x1000/8);

    init_mapper.virt_offset = hhdm_request.response->offset;
    global_temp_mapper = &init_mapper;
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

void construct_paging() {
    u64 cr3 = (u64)kernel_pframe_allocator.alloc_page();

    // Map kernel pages
    //
    // Prelude:
    // $ misha@Yoga:~/pmos/kernel/build$ readelf -l ../kernel
    //
    // Elf file type is EXEC (Executable file)
    // Entry point 0xffffffff80000247
    // There are 6 program headers, starting at offset 64
    //
    // Program Headers:
    //   Type           Offset             VirtAddr           PhysAddr
    //                  FileSiz            MemSiz              Flags  Align
    //   LOAD           0x0000000000200000 0xffffffff80000000 0xffffffff80000000
    //                  0x00000000000575dc 0x00000000000575dc  R E    0x1000
    //   LOAD           0x0000000000258000 0xffffffff80058000 0xffffffff80058000
    //                  0x00000000000078e0 0x00000000000078e0  R      0x1000
    //   LOAD           0x0000000000260000 0xffffffff80060000 0xffffffff80060000
    //                  0x00000000000044b8 0x00000000000044b8  RW     0x1000
    //   LOAD           0x00000000000644c0 0xffffffff800644c0 0xffffffff800644c0
    //                  0x0000000000000000 0x000000000000922c  RW     0x1000
    //   LOAD           0x000000000026e000 0xffffffff8006e000 0xffffffff8006e000
    //                  0x0000000000009798 0x0000000000009798  R      0x1000
    //   LOAD           0x0000000000277798 0xffffffff80077798 0xffffffff80077798
    //                  0x000000000000198e 0x000000000000198e  R      0x1000
    // 
    // Section to Segment mapping:
    //     Segment Sections...
    //     00     .text .init .fini
    //     01     .rodata
    //     02     .data .ctors .dtors
    //     03     .bss
    //     04     .eh_frame
    //     05     .gcc_except_table
    //
    //
    //
    // The kernel is loaded into higher half and has 6 memory regions (4 of which could be merged into 2):
    // 1. .text and related, with Read an Execute permissions
    // 2. .rodata, with Read permissions
    // 3. .data and .bss, with Read and Write permissions
    // 4. .eh_frame and .gcc_except_table, with Read only permissions
    // The addresses of these sections are known from the symbols, defined by linker script and their physical
    // location can be obtained from Kernel Address Feature Request by limine protocol
    // Map these pages and switch to kernel page table
    const u64 kernel_start_virt = (u64)&_kernel_start & ~0xfff;
    const u64 kernel_text_start = kernel_start_virt & ~0xfff;
    const u64 kernel_text_end = ((u64)&_text_end + 0xfff) & ~0xfff;
    
    const u64 kernel_phys = kernel_address_request.response->physical_base;
    const u64 text_phys = kernel_phys + kernel_text_start - kernel_start_virt;
    const u64 text_size = kernel_text_end - kernel_text_start;
    const u64 text_virt = text_phys - kernel_phys + kernel_start_virt;
    Page_Table_Argumments args = {
        .writeable = false,
        .user_access = false,
        .global = true,
        .execution_disabled = false,
        .extra = 0,
    };
    u64 result = map_pages(text_phys, text_virt, text_size, args, cr3);
    if (result != SUCCESS)
        hcf();

    const u64 rodata_start = (u64)(&_rodata_start) & ~0xfff;
    const u64 rodata_end = ((u64)&_rodata_end + 0xfff) & ~0xfff;
    const u64 rodata_size = rodata_end - rodata_start;
    const u64 rodata_offset = rodata_start - kernel_start_virt;
    const u64 rodata_phys = kernel_phys + rodata_offset;
    const u64 rodata_virt = kernel_start_virt + rodata_offset;
    args = {false, false, true, true, 0};
    result = map_pages(rodata_phys, rodata_virt, rodata_size, args, cr3);
    if (result != SUCCESS)
        hcf();

    const u64 data_start = (u64)(&_data_start) & ~0xfff;
    // Data and BSS are merged and have the same permissions
    // And linker script doesn't align their boundary to page
    // So merge them together
    const u64 data_end = ((u64)&_bss_end + 0xfff) & ~0xfff;
    const u64 data_size = data_end - data_start;
    const u64 data_offset = data_start - kernel_start_virt;
    const u64 data_phys = kernel_phys + data_offset;
    const u64 data_virt = kernel_start_virt + data_offset;
    args = {true, false, true, true, 0};
    result = map_pages(data_phys, data_virt, data_size, args, cr3);
    if (result != SUCCESS)
        hcf();

    const u64 eh_frame_start = (u64)(&__eh_frame_start) & ~0xfff;
    // Same as with data; merge eh_frame and gcc_except_table
    const u64 eh_frame_end = ((u64)&_gcc_except_table_end + 0xfff) & ~0xfff;
    const u64 eh_frame_size = eh_frame_end - eh_frame_start;
    const u64 eh_frame_offset = eh_frame_start - kernel_start_virt;
    const u64 eh_frame_phys = kernel_phys + eh_frame_offset;
    const u64 eh_frame_virt = kernel_start_virt + eh_frame_offset;
    args = {true, false, true, true, 0};
    result = map_pages(eh_frame_phys, eh_frame_virt, eh_frame_size, args, cr3);
    if (result != SUCCESS)
        hcf();

    setCR3(cr3);
}


void limine_main() {
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    init_memory();
    construct_paging();

    asm ("xchgw %bx, %bx");
}