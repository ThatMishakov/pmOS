#pragma once
#include <types.hh>
#include <lib/vector.hh>
#include <lib/string.hh>
#include <pmos/load_data.h>
#include <memory/mem_object.hh>

constexpr phys_addr_t RSDP_INITIALIZER = -1ULL;
extern phys_addr_t rsdp;

void init_acpi(phys_addr_t rsdp_addr);

struct module {
    klib::string path;
    klib::string cmdline;

    u64 phys_addr;
    u64 size;

    klib::shared_ptr<kernel::paging::Mem_Object> object;
};

extern klib::vector<module> modules;

klib::unique_ptr<load_tag_generic> construct_load_tag_for_modules();