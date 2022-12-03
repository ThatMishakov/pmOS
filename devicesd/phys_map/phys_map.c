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

void list_insert_after(map_list* element, map_list* to_insert)
{
    to_insert->next = element->next;
    element->next = to_insert;
}

static void map_phys_push_free(uint64_t virt_addr_start, uint64_t number_of_pages)
{
    // TODO
    return;
    // TODO: Ordering and merging (or preferably a better data structure)
    map_list* l = malloc(sizeof(map_list));
    l->start = (void*)virt_addr_start;
    l->pages = number_of_pages;
    l->next = NULL;

    list_insert_after(&free_pages_head, l);
}

void* map_phys(void* phys_addr, size_t bytes)
{
    // TODO: It looks like it leaks memory

    uint64_t addr_alligned = (uint64_t)phys_addr & ~(uint64_t)07777;
    uint64_t end_alligned = (uint64_t)phys_addr + bytes;
             end_alligned = (end_alligned & ~(uint64_t)07777) + (end_alligned & (uint64_t)07777 ? 010000 : 0);
    size_t size_pages = (end_alligned - addr_alligned) >> 12;


    map_list* p = &free_pages_head;
    while (p->next != NULL && p->next->pages < size_pages)
        p = p->next;

    uint64_t virt_addr_start = 0;
    if (p->next == NULL) {
        virt_addr_start = (uint64_t)heap_reserve_pages(size_pages);
    } else { // Traverse list

    }

    //printf("Syscall virt_addr_start %lX addr_alligned %lX size_pages %li\n", virt_addr_start, (uint64_t)addr_alligned, size_pages);
    result_t result = syscall_map_phys(virt_addr_start, (uint64_t)addr_alligned, size_pages, FLAG_RW | FLAG_NOEXECUTE).result;

    if (result != SUCCESS) {
        map_phys_push_free(virt_addr_start, size_pages);
        return NULL;
    }

    return (void*)virt_addr_start + ((uint64_t)phys_addr & 07777);
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