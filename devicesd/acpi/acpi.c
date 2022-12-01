#include <acpi/acpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <phys_map/phys_map.h>
#include <lai/core.h>
#include <string.h>

RSDP_descriptor* rsdp_desc = NULL;
RSDP_descriptor20* rsdp20_desc = NULL;

typedef struct acpi_mem_map_node {
    void* phys;
    size_t size_bytes;
    void* virt;
    struct acpi_mem_map_node* next;
} acpi_mem_map_node;

acpi_mem_map_node acpi_mem_map_list_dummy = {0, 0, 0, NULL};

void acpi_mem_map_list_push_front(acpi_mem_map_node* n)
{
    n->next = acpi_mem_map_list_dummy.next;
    acpi_mem_map_list_dummy.next = n;
}

void* acpi_map_and_get_virt(void* phys, size_t size)
{
    for (acpi_mem_map_node* p = acpi_mem_map_list_dummy.next; p != NULL; p = p->next) {

        if (p->phys <= phys && (p->size_bytes >= size + (((size_t)phys - (size_t)p->phys))))
            return  (void*)((uint64_t)phys - (uint64_t)p->phys + (uint64_t)p->virt);
    }

    void* virt = map_phys(phys, size);
    // if (virt == NULL) panic("Panic: Could not map meory\n");

    acpi_mem_map_node* n = malloc(sizeof(acpi_mem_map_node));
    // if (n == NULL) panic("Panic: Could not allocate memory\n");

    n->phys = phys;
    n->size_bytes = size;
    n->virt = virt;
    n->next = NULL;
    acpi_mem_map_list_push_front(n);
    return virt;
}

void acpi_map_release_exact(void* virt, size_t size)
{
    acpi_mem_map_node* p = &acpi_mem_map_list_dummy;
    for (; p->next != NULL; p = p->next) {
        if (p->next->virt == virt && p->next->size_bytes == size) break;
    }

    acpi_mem_map_node* temp = p->next->next;
    unmap_phys(p->next->virt, p->next->size_bytes);
    free(p->next);
    p->next = temp;    
}

RSDT* rsdt_phys = NULL;

XSDT* xsdt_phys = NULL;

void init_acpi()
{
    printf("Info: Initializing ACPI...\n");
    int acpi_rev = 0;

    // TODO: Checksums
    if (rsdp20_desc != NULL) {
        RSDP_descriptor20* rsdp20_desc_virt = acpi_map_and_get_virt(rsdp20_desc, sizeof(RSDP_descriptor20));
        acpi_rev = rsdp20_desc_virt->firstPart.revision;

        xsdt_phys = rsdp20_desc_virt->xsdt_address;
        // TODO: checksum     
        goto acpi_tables_ok;
    } 
    
    if (rsdp_desc != NULL) {
        RSDP_descriptor* rsdp_desc_virt = acpi_map_and_get_virt(rsdp_desc, sizeof(rsdp_desc));
        acpi_rev = rsdp_desc_virt->revision;

        rsdt_phys = rsdp_desc_virt->rsdt_address;
        goto acpi_tables_ok;
    }

acpi_tables_ok:
    printf("Mapped tables. ACPI revision: %i\n", acpi_rev);

    lai_set_acpi_revision(acpi_rev);
    lai_create_namespace();

    printf("Inited lai\n"); 
}

ACPISDTHeader* get_table(const char* signature, int n)
{
    ACPISDTHeader* h = NULL;
    ACPISDTHeader* phys = NULL;
    int i = 0;

    if (xsdt_phys != 0) {
        XSDT *xsdt = map_phys(xsdt_phys, sizeof(XSDT));
        uint32_t length = xsdt->h.length;
        xsdt = map_phys(xsdt_phys, length);
        uint32_t entries = (length - sizeof(xsdt->h)) / 8;

        for (uint32_t i = 0; i < entries; ++i) {
            phys = (ACPISDTHeader *) xsdt->PointerToOtherSDT[i];
            h = map_phys(phys, sizeof(ACPISDTHeader));
            if (!strncmp(h->signature, signature, 4)) {
                if (i++ == n) break;
            }
            h = NULL;
        }
    } else if (rsdt_phys != 0) {
        RSDT *rsdt = map_phys(rsdt_phys, sizeof(RSDT));
        uint32_t length = rsdt->h.length;
        rsdt = map_phys(rsdt_phys, length);
        uint32_t entries = (length - sizeof(rsdt->h)) / 4;

        for (uint32_t i = 0; i < entries; ++i) {
            phys = (ACPISDTHeader *) rsdt->PointerToOtherSDT[i];
            h = map_phys(phys, sizeof(ACPISDTHeader));
            if (!strncmp(h->signature, signature, 4)) {
                if (i++ == n) break;
            }
            h = NULL;
        }
    }

    if (h == NULL) return NULL;
    // TODO: Check signature

    uint32_t length = h->length;
    h = map_phys(phys, length);

    return h;
}