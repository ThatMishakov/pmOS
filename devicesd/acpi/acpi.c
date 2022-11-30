#include <acpi/acpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <phys_map/phys_map.h>

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

RSDT* rsdt = NULL;
XSDT* xsdt = NULL;

void init_acpi()
{
    printf("Info: Initializing ACPI...\n");

    if (rsdp20_desc != NULL) {
        RSDP_descriptor20* rsdp20_desc_virt = acpi_map_and_get_virt(rsdp20_desc, sizeof(RSDP_descriptor20));
    }

}