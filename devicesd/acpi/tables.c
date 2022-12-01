#include <acpi/acpi.h>
#include <string.h>

typedef struct ACPI_Ind_E
{
    ACPIList head;
    char signature[4];
} ACPI_Ind_E;

ACPI_Ind_E acpi_tables[] = {
    {{0,0}, "APIC"},
    {{0,0}, "BERT"},
    {{0,0}, "CPEP"},
    {{0,0}, "DSDT"},
    {{0,0}, "ECDT"},
    {{0,0}, "EINJ"},
    {{0,0}, "ERST"},
    {{0,0}, "FACP"},
    {{0,0}, "FACS"},
    {{0,0}, "HEST"},
    {{0,0}, "MSCT"},
    {{0,0}, "MPST"},
    {{0,0}, "PMTT"},
    {{0,0}, "PSDT"},
    {{0,0}, "RASF"},
    {{0,0}, "RSDT"},
    {{0,0}, "SBST"},
    {{0,0}, "SLIT"},
    {{0,0}, "SRAT"},
    {{0,0}, "SSDT"},
    {{0,0}, "XSDT"},
    };

ACPIList OEM_Tables = {0, 0};

ACPISDTHeader* acpi_head_get_index(ACPIList* head, int n)
{
    ACPIList* p = head;
    for(; p->next != NULL && n--;p = p->next) ;

    if (p->next != NULL) return p->next->virt_addr;
    return NULL;
}

ACPISDTHeader* acpi_head_get_index_signature(ACPIList* head, const char* signature, int n)
{
    // TODO
    return NULL;
}

ACPISDTHeader* get_table(const char* signature, int n)
{
    const char* p = signature;
    if (!strncmp(signature, "OEM", 3)) {
        return acpi_head_get_index_signature(&OEM_Tables, signature, n);
    }

    ACPIList *head = NULL;

    for (int i = 0; i < sizeof(acpi_tables)/sizeof(ACPI_Ind_E); ++i)
        if (!strncmp(acpi_tables[i].signature, signature, 4)) {
            head = &(acpi_tables[i].head);
        }

    if (head == NULL) return NULL;

    return acpi_head_get_index(head, n);
}

// Allocates virtual address (of the right size) from physical address of the table
// Returns NULL is checksum is not valid or couldn't map to process memory
ACPISDTHeader* check_get_virt_from_phys(ACPISDTHeader* phys);

// Returns -1 on error or ACPI version otherwise 
int walk_acpi_tables();