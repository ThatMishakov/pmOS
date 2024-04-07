#include <pci/pci.h>
#include <stdio.h>
#include <acpi/acpi.h>

void init_pci() {
    printf("Iitializing PCI...\n");

    MCFG *t = (MCFG *)get_table("MCFG", 0);
    if (!t) {
        printf("Warning: Could not find MCFG table...\n");
        return;
    }

    int c = MCFG_list_size(t);
    for (int i = 0; i < c; ++i) {
        MCFGBase *b = &t->structs_list[i];
        printf("Table %i base addr %lx group %i start %i end %i\n", i, b->base_addr, b->group_number, b->start_bus_number, b->end_bus_number);
    }
}