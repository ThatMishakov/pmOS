#pragma once
#include <types.hh>

/// Enumerate all ACPI tables from the RSDT
void enumerate_tables(u64 rsdt_desc_phys);

/// Returns the physical address of the table with the given signature
/// 0 is returned if the table was not found (or to an extension, ACPI is not available)
u64 get_table(u32 signature);