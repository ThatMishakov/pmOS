#include "init.h"
#include <pmos/vector.h>
#include <stdlib.h>
#include <pmos/pmbus_object.h>
#include <string.h>
#include "../io.h"
#include <errno.h>
#include <elf.h>
#include <pmos/system.h>
#include <pmos/memory.h>
#include <pmos/load_data.h>
#include <elf.h>
#include "../elf/loader.h"
#include <inttypes.h>

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

uint64_t instance_id_counter = 1;

int start_service(struct Service *service, uint64_t object_id, uint64_t optional_right_id)
{
    if (!service)
        return -EINVAL;

    int result = 0;
    size_t size = service->instances.size;
    VECTOR_RESERVE(service->instances, size + 1, result);
    if (result != 0) {
        print_str("Loader: Could not reserve space for service instance for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(result);
        print_str("\n");
        return -ENOMEM;
    }

    uint64_t group_id = {};
    syscall_r r       = syscall_new_process();
    void *mem_region  = NULL;
    if (r.result != SUCCESS) {
        print_str("Loader: Could not create process for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(r.result);
        print_str("\n");

        return r.result;
    }

    // Doesn't really matter if this fails
    syscall_set_task_name(r.value, service->name, strlen(service->name));

    syscall_r rr = create_task_group();
    if (rr.result != SUCCESS) {
        print_str("Loader: Could not create task group for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(rr.result);
        print_str("\n");

        syscall_kill_task(r.value);
        return rr.result;
    }

    group_id = rr.value;

    result_t rrr = add_task_to_group(r.value, group_id);
    if (rrr != SUCCESS) {
        print_str("Loader: Could not add task to group for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(r.result);
        print_str("\n");

        syscall_kill_task(r.value);
        remove_task_from_group(TASK_ID_SELF, group_id);
        return rrr;
    }

    pmos_right_t new_right = 0;
    if (optional_right_id) {
        auto result = transfer_right(group_id, optional_right_id, 0);
        if (result.result) {
            print_str("Loader: Could not transfer right to the task group for service ");
            print_str(service->name);
            print_str(". Error: ");
            print_hex(r.result);
            print_str("\n");

            syscall_kill_task(r.value);
            remove_task_from_group(TASK_ID_SELF, group_id);
            return result.result;
        }
        new_right = result.right;
    }

    uint64_t new_group_id = group_id;
    remove_task_from_group(TASK_ID_SELF, group_id);
    group_id = 0;

    // Pass arguments
    size_t args_size = 0;
    args_size += sizeof(struct load_tag_task_group_id);
    // TODO: Add other tags (and move this to a separate function, this is a mess)
    args_size += sizeof(struct load_tag_close);

    size_t page_aligned = (args_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    mem_request_ret_t mem =
        create_normal_region(TASK_ID_SELF, NULL, page_aligned, PROT_READ | PROT_WRITE);
    if (mem.result != SUCCESS) {
        print_str("Loader: Could not create memory region for arguments for ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(mem.result);
        print_str("\n");

        result = mem.result;
        goto error;
    }
    mem_region = mem.virt_addr;

    size_t current_offset           = 0;
    struct load_tag_task_group_id g = {
        .header   = LOAD_TAG_TASK_GROUP_ID_HEADER,
        .group_id = new_group_id,
    };
    memcpy((char *)mem_region + current_offset, &g, sizeof(g));
    current_offset += sizeof(g);

    struct load_tag_close h = {
        .header = LOAD_TAG_CLOSE_HEADER,
    };
    memcpy((char *)mem_region + current_offset, &h, sizeof(h));

    // Task group tag
    struct AuxVecEntry *auxvec_entries[4];
    struct AuxVecEntry group_id_entry = {
        .entry_type = AT_TASK_GROUP_ID,
        .data_type = DATA_TYPE_EXTERNAL,
        .external_data = {
            .data = &new_group_id,
            .size = sizeof(new_group_id),
        },
    };
    auxvec_entries[0] = &group_id_entry;
    auxvec_entries[1] = NULL;

    const char *argc[5];
    argc[0] = service->name;
    argc[1] = NULL;

    if (new_right) {
        argc[1] = "--right-id";
        char buff[64];
        sprintf(buff, "%" PRIu64, new_right);
        argc[2] = buff;
        argc[3] = NULL;
    }

    result_t res = load_executable(r.value, object_id, 0, mem_region, page_aligned, argc, NULL, (const struct AuxVecEntry **)auxvec_entries);
    //result_t res = syscall_load_executable(r.value, object_id, mem_region, 0);
    if (res != SUCCESS) {
        print_str("Loader: Could not load executable ");
        print_str(service->name);
        print_str(". Error: ");
        print_hex(res);
        print_str("\n");
        result = res;

        goto error;
    }

    service->state = STATE_STARTED;
    mem_region        = NULL;

    struct Instance instance = {
        .id = instance_id_counter++,
        .group_id = new_group_id,
        .parent = service,
        .state = STATE_STARTED,
    };

    VECTOR_PUSH_BACK(service->instances, instance);

    return 0;
error:
    if (mem_region)
        release_region(TASK_ID_SELF, mem_region);

    if (group_id)
        remove_task_from_group(TASK_ID_SELF, group_id);

    if (r.value)
        syscall_kill_task(r.value);

    return result;
}

struct Service *services = NULL;

void push_services(struct Service *s)
{
    // Leak that memory!
    while (s) {
        struct Service *ss = s->next;
        s->next = services;
        services = s;

        print_str("Added service ");
        if (s->name)
            print_str(s->name);
        print_str("\n");

        s = ss;
    }
}

struct module_descriptor_list {
    struct module_descriptor_list *next;
    uint64_t object_id;
    size_t size;
    char *cmdline;
    char *path;
    struct Service *service;
};

struct module_descriptor_list *find_module(char *path);

void match_services()
{
    for (struct Service *s = services; s; s = s->next) {
        if (s->module)
            continue;

        const char *name;
        if (s->name)
            name = s->name;
        else
            name = "<unknown>";

        if (!s->path) {
            print_str("Warning: module ");
            print_str(name);
            print_str(" with empty path!\n");
            continue;
        }

        struct module_descriptor_list *module = find_module(s->path);
        if (!module) {
            print_str("Module not found for service ");
            print_str(name);
            print_str(", path: ");
            print_str(s->path);
            print_str("\n");
            continue;
        }

        if (module->service) {
            print_str("Warning: module ");
            print_str(module->path);
            print_str(" already has a service!\n");
            continue;
        }

        module->service = s;
        s->module = module;
    }
}