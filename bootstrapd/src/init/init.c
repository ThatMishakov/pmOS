#include "init.h"
#include <pmos/vector.h>
#include <stdlib.h>
#include <pmos/pmbus_object.h>
#include <string.h>
#include "../io.h"

static void free_match_filters(match_filter_vector filters)
{
    struct MatchFilter f;
    VECTOR_FOREACH(filters, f) {
        free(f.key);
        if (f.strings) {
            char **s = f.strings;
            while (*s) {
                free(*s);
                ++s;
            }
            free(f.strings);
        }
    }
    VECTOR_FREE(filters);
}

void free_service(struct Service *service)
{
    if (!service)
        return;

    free(service->name);
    free(service->path);
    free(service->description);
    VECTOR_FREE(service->instances);

    struct Requirement r;
    VECTOR_FOREACH(service->requirements, r) {
        free(r.service_name);
    }
    VECTOR_FREE(service->requirements);

    free_match_filters(service->match_filters);

    free(service);
}

struct Service *new_service()
{
    return calloc(1, sizeof(struct Service));
}

void *construct_pci_filter(struct PCIFilter f)
{
    pmos_bus_filter_conjunction *c = pmos_bus_filter_conjunction_create();
    if (!c)
        return NULL;

    pmos_bus_filter_equals *e = pmos_bus_filter_equals_create("device_type", "pci");
    if (!e) {
        print_str("Failed to create PCI device filter\n");
        goto error;
    }
    if (pmos_bus_filter_conjunction_add(c, e) != 0) {
        print_str("Failed to add value to the conjunction\n");
        pmos_bus_filter_free(e);
        goto error;
    }

    if (f.class) {
        e = pmos_bus_filter_equals_create("pci_class", f.class);
        if (!e) {
            print_str("Failed to create PCI device filter\n");
            goto error;
        }
        if (pmos_bus_filter_conjunction_add(c, e) != 0) {
            print_str("Failed to add value to the conjunction\n");
            pmos_bus_filter_free(e);
            goto error;
        }
    }
    if (f.prog_if) {
        e = pmos_bus_filter_equals_create("pci_interface", f.prog_if);
        if (!e) {
            print_str("Failed to create PCI device filter\n");
            goto error;
        }
        if (pmos_bus_filter_conjunction_add(c, e) != 0) {
            print_str("Failed to add value to the conjunction\n");
            pmos_bus_filter_free(e);
            goto error;
        }
    }
    if (f.subclass) {
        e = pmos_bus_filter_equals_create("pci_subclass", f.subclass);
        if (!e) {
            print_str("Failed to create PCI device filter\n");
            goto error;
        }
        if (pmos_bus_filter_conjunction_add(c, e) != 0) {
            print_str("Failed to add value to the conjunction\n");
            pmos_bus_filter_free(e);
            goto error;
        }
    }

return c;

error:
    pmos_bus_filter_free(c);
    return NULL;
}

void *construct_acpi_filter(char **strings)
{
    if (!strings || !strings[0])
        return NULL;

    pmos_bus_filter_disjunction *c = pmos_bus_filter_disjunction_create();
    if (!c)
        return NULL;

    char **s = strings;
    while (*s) {
        pmos_bus_filter_equals *e = NULL;

        e = pmos_bus_filter_equals_create("acpi_hid", *s);
        if (!e) {
            print_str("Failed to create ACPI HID filter\n");
            goto error;
        }
        if (pmos_bus_filter_disjunction_add(c, e) != 0) {
            print_str("Failed to add ACPI HID filter to disjunction\n");
            pmos_bus_filter_free(e);
            goto error;
        }

        e = pmos_bus_filter_equals_create("acpi_cid", *s);
        if (!e) {
            print_str("Failed to create ACPI CID filter\n");
            goto error;
        }

        if (pmos_bus_filter_disjunction_add(c, e) != 0) {
            print_str("Failed to add ACPI CID filter to disjunction\n");
            pmos_bus_filter_free(e);
            goto error;
        }

        ++s;
    }

    return c;
error:
    pmos_bus_filter_disjunction_free(c);
    return NULL;
}


void *construct_filter(struct Service *service)
{
    pmos_bus_filter_disjunction *d = pmos_bus_filter_disjunction_create();
    if (!d)
        return NULL;

    size_t entries = 0;
    const struct MatchFilter *f;
    VECTOR_FOREACH_PTR(service->match_filters, f) {
        if (!f->key) {
            print_str("Match filter with NULL key in service ");
            print_str(service->name);
            print_str("\n");
            continue;
        }
        if (!f->strings || !f->strings[0]) {
            print_str("Match filter with no strings in service ");
            print_str(service->name);
            print_str("\n");
            continue;
        }

        if (!strcmp(f->key, "pnp")) {
            void *a = construct_acpi_filter(f->strings);
            if (!a) {
                print_str("ACPI filter error (NULL) for service ");
                print_str(service->name);
                print_str("\n");
                continue;
            }

            if (pmos_bus_filter_disjunction_add(d, a) < 0) {
                print_str("Failed to add ACPI filter to disjunction\n");
                pmos_bus_filter_free(a);
                goto error;
            }
        } else if (!strcmp(f->key, "pci")) {
            void *a = construct_pci_filter(f->pci_filter);
            if (!a) {
                print_str("PCI filter error (NULL) for service ");
                print_str(service->name);
                print_str("\n");
                continue;
            }

            if (pmos_bus_filter_disjunction_add(d, a) < 0) {
                print_str("Failed to add ACPI filter to disjunction\n");
                pmos_bus_filter_free(a);
                goto error;
            }
        } else {
            print_str("Unknown filter key: ");
            print_str(f->key);
            print_str("\n");
            continue;
        }

        entries++;
    }

    if (entries == 0) {
        print_str("Service has no valid match filters\n");
        goto error;
    }

    return d;
error:
    pmos_bus_filter_free(d);
    return NULL;
}