#pragma once
#include <acpi/acpi.h>

/// Get MADT table, mapped in the kernel address space
/// If nullptr is returned, then it is not present or ACPI
/// is not supported by the system
MADT * get_madt();

RCHT * get_rhct();