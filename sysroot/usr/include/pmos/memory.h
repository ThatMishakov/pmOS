#ifndef _PMOS_MEMORY_H
#define _PMOS_MEMORY_H
#include "memory_flags.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct mem_request_ret_t {
    result_t result;
    void *virt_addr;
} mem_request_ret_t;


#ifdef __STDC_HOSTED__
mem_request_ret_t create_normal_region(uint64_t pid, void *addr_start, size_t size, uint64_t access);

mem_request_ret_t create_managed_region(uint64_t pid, void *addr_start, size_t size, uint64_t access, pmos_port_t port);

mem_request_ret_t create_phys_map_region(uint64_t pid, void *addr_start, size_t size, uint64_t access, void* phys_addr);

typedef uint64_t pmos_pagetable_t;
typedef struct page_table_req_ret_t {
    result_t result;
    pmos_pagetable_t page_table;
} page_table_req_ret_t;

page_table_req_ret_t get_page_table(uint64_t pid);

result_t provide_page(uint64_t page_table, uint64_t dest_page, uint64_t source, uint64_t flags);

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif