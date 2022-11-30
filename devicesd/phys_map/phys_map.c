#include <phys_map/phys_map.h>
#include <stdint.h>
#include <pmos/special.h>
#include <kernel/errors.h>
#include <pmos/system.h>
#include <kernel/flags.h>

typedef struct map_list {
    void* start;
    size_t pages;
    struct map_list* next;
} map_list;

map_list free_pages_head = {0, 0, NULL};

static void map_phys_push_free(uint64_t virt_addr_start, uint64_t number_of_pages)
{
    // TODO
}

void* map_phys(void* phys_addr, size_t bytes)
{
    uint64_t addr_alligned = (uint64_t)phys_addr & ~(uint64_t)0777;
    uint64_t end_alligned = (uint64_t)phys_addr + bytes;
             end_alligned = (end_alligned & ~(uint64_t)0777) + (end_alligned & (uint64_t)0777 ? 01000 : 0);
    size_t size_pages = (end_alligned - addr_alligned) >> 12;


    map_list* p = &free_pages_head;
    while (p->next != NULL && p->next->pages < size_pages)
        p = p->next;

    uint64_t virt_addr_start = 0;
    if (p->next == NULL) {
        virt_addr_start = (uint64_t)heap_reserve_pages(size_pages);
    } else { // Traverse list

    }

    result_t result = syscall_map_phys(virt_addr_start, (uint64_t)phys_addr, size_pages, FLAG_RW | FLAG_NOEXECUTE).result;

    if (result != SUCCESS) {
        map_phys_push_free(virt_addr_start, size_pages);
        return NULL;
    }

    return (void*)virt_addr_start + ((uint64_t)phys_addr & 0777);
}

void unmap_phys(void* virt_addr, size_t bytes)
{
    uint64_t addr_alligned = (uint64_t)virt_addr & ~(uint64_t)0777;
    uint64_t end_alligned = (uint64_t)virt_addr + bytes;
             end_alligned = (end_alligned & ~(uint64_t)0777) + (end_alligned & (uint64_t)0777 ? 01000 : 0);
    size_t size_pages = (end_alligned - addr_alligned) >> 12;
    
    result_t r = syscall_release_page_multi(addr_alligned, size_pages);
    if (r == SUCCESS) {
        map_phys_push_free(addr_alligned, size_pages);
    }
}