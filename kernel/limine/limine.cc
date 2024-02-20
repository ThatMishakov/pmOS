#include "limine.h"
#include "../memory/mem.hh"

void limine_main();

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
    .entry = limine_main,
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
}



void limine_main() {
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    init_memory();

    asm ("xchgw %bx, %bx");
}