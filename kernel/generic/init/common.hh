#pragma once
#include <types.hh>

constexpr phys_addr_t RSDP_INITIALIZER = -1ULL;
extern phys_addr_t rsdp;

void init_acpi(phys_addr_t rsdp_addr);