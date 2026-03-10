#pragma once
#include <pmos/pmbus_object.h>

struct PCIDevice;

int register_pci_object(pmos_bus_object_t *object_ownin, struct PCIDevice *device);
int register_acpi_object(pmos_bus_object_t *object_owning);