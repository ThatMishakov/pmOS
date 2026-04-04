#pragma once
#include <uacpi/namespace.h>
#include <uacpi/resources.h>
#include <uacpi/uacpi.h>
#include <uacpi/utilities.h>

typedef struct acpi_driver {
    const char *device_name;
    const char *const *pnp_ids; // NULL-terminated list of compatible IDs
    int (*device_probe)(uacpi_namespace_node *node, uacpi_namespace_node_info *info);

    struct acpi_driver *next;
} acpi_driver;

void acpi_register_driver(struct acpi_driver *driver);