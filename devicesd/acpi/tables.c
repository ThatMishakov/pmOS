#include <acpi/acpi.h>
#include <string.h>
#include <phys_map/phys_map.h>
#include <panic.h>
#include <stdio.h>


typedef struct ACPITreeNode {
  ACPISDTHeader* virt_addr;
  struct ACPITreeNode* left;
  struct ACPITreeNode* right;
} ACPITreeNode;

static ACPITreeNode* root;

ACPISDTHeader* get_table(const char* signature, int n)
{
    if (!root) return NULL;

    ACPITreeNode* ptr = root;
    ACPITreeNode* t = NULL;

    while (ptr) {   
        int cmp = strncmp(ptr->virt_addr->signature, signature, 4);
        if (cmp == 0 && !n--) {
            t = ptr;
            break;
        }

        if (cmp < 0) {
            ptr = ptr->left;
        } else {
            ptr = ptr->right;
        }
    }

    return t ? t->virt_addr : NULL;
}

void push_table(ACPISDTHeader* acpi_table_virt)
{
    ACPITreeNode* node = malloc(sizeof(ACPITreeNode));
    if (node == NULL) {
        // panic("Error: Could not allocate memory\n");
        exit(2);
    }

    node->left = NULL;
    node->right = NULL;
    node->virt_addr = acpi_table_virt;

    if (!root) {
        root = node;
    } else {
        ACPITreeNode* ptr = root;
        int cmp = strncmp(ptr->virt_addr->signature, acpi_table_virt->signature, 4);

        while (1) {
            if (cmp < 0) {
                if (ptr->left) {
                    ptr = ptr->left;
                } else {
                    ptr->left = node;
                    break;
                }
            } else {
                if (ptr->right) {
                    ptr = ptr->right;
                } else {
                    ptr->right = node;
                    break;
                }
            }
            cmp = strncmp(ptr->virt_addr->signature, acpi_table_virt->signature, 4);
        }
    }
}

// Allocates virtual address (of the right size) from physical address of the table
// Returns NULL is checksum is not valid or couldn't map to process memory
ACPISDTHeader* check_get_virt_from_phys(ACPISDTHeader* phys)
{
    ACPISDTHeader* virt_small = map_phys(phys, sizeof(ACPISDTHeader*));
    uint32_t size = virt_small->length;
    unmap_phys(virt_small, sizeof(ACPISDTHeader*));   


    // TODO: I don't know what is a sane value as I've seen some *huge* tables on my laptop
    // // Check if size is sane
    // if (size > 0x1000000)
    //     return NULL;

    ACPISDTHeader* virt_big = map_phys(phys, size);
    if (check_table(virt_big)) {
        return virt_big;
    }

    // On error
    unmap_phys(virt_big, size);
    return NULL;
}

RSDT* get_rsdt_from_desc(RSDP_descriptor* rsdp_desc_phys)
{
    RSDP_descriptor* rsdp_virt = map_phys(rsdp_desc_phys, sizeof(RSDP_descriptor));

    if (rsdp_virt == NULL) {
        // panic("Could not map rsdp\n");
    }

    unsigned char sum = 0;
    for (size_t i = 0; i < sizeof(RSDP_descriptor); ++i)
        sum += ((unsigned char*)rsdp_virt)[i];

    if (sum || strncmp("RSD PTR ", rsdp_virt->signature, 8)) {
        fprintf(stderr, "Warning: RSDP Descriptor not valid\n");
        return NULL;
    }

    ACPISDTHeader *rsdt_phys = (ACPISDTHeader*)(uint64_t)rsdp_virt->rsdt_address; // TODO: Will need to be changed when porting to 32 bit
    RSDT* rsdt_virt = (RSDT*)check_get_virt_from_phys(rsdt_phys);

    if (!rsdt_virt || strncmp("RSDT", rsdt_virt->h.signature, 4)) {
        fprintf(stderr, "Warning: RSDT pointed by RSDP not valid\n");
        return NULL;
    }

    return rsdt_virt;
}

XSDT* get_xsdt_from_desc(RSDP_descriptor20* rsdp_desc_phys)
{
    RSDP_descriptor20* rsdp_virt = map_phys(rsdp_desc_phys, sizeof(RSDP_descriptor20));

    if (rsdp_virt == NULL) {
        // panic("Could not map rsdp\n");
    }

    unsigned char sum = 0;
    for (size_t i = 0; i < sizeof(RSDP_descriptor20); ++i)
        sum += ((unsigned char*)rsdp_virt)[i];

    if (sum || strncmp("RSD PTR ", rsdp_virt->firstPart.signature, 8)) {
        fprintf(stderr, "Warning: RSDP Descriptor not valid\n");
        return NULL;
    }

    ACPISDTHeader *xsdt_phys = (ACPISDTHeader*)(uint64_t)rsdp_virt->xsdt_address; // TODO: Will need to be changed when porting to 32 bit
    XSDT* xsdt_virt = (XSDT*)check_get_virt_from_phys(xsdt_phys);

    if (!xsdt_virt || strncmp("XSDT", xsdt_virt->h.signature, 4)) {
        fprintf(stderr, "Warning: XSDT pointed by RSDP not valid\n");
        return NULL;
    }

    return xsdt_virt;
}

void parse_fadt(FADT* fadt_virt)
{
    uint8_t revision = fadt_virt->h.revision;

    // TODO: FACS
    ACPISDTHeader* dsdt_phys = NULL;

    if (revision < 2) {
        dsdt_phys = (ACPISDTHeader*)(uint64_t)fadt_virt->dsdt_phys;
    } else {
        dsdt_phys = (ACPISDTHeader*)fadt_virt->X_DSDT;
    }

    DSDT* dsdt_virt = (DSDT*)check_get_virt_from_phys(dsdt_phys);
    if (dsdt_virt == NULL || strncmp(dsdt_virt->h.signature, "DSDT", 4)) {
        fprintf(stderr,"Panic: Weird DSDT table pointer in FADT table, dsdt_phys %lx dsdt_virt: %lx revision: %i\n", (uint64_t)dsdt_phys, (uint64_t)dsdt_virt, revision);
        exit(2);
    }

    push_table((ACPISDTHeader*)dsdt_virt);
}

// Returns -1 on error or ACPI version otherwise 
int walk_acpi_tables()
{

    char xsdt_enumerated = 0;
    if (rsdp20_desc != NULL) { // TODO
        XSDT* xsdt = get_xsdt_from_desc(rsdp20_desc);
        if (xsdt == NULL) {
            fprintf(stderr, "Warning: could not validade XSDT table %lX\n", (uint64_t)xsdt);      
        } else {
            xsdt_enumerated = 1;
            push_table((ACPISDTHeader*)xsdt);
            int length = xsdt->h.length;
            uint32_t entries = (length - sizeof(xsdt->h))/sizeof(uint64_t);
            for (uint32_t i = 0; i < entries; ++i) {
                ACPISDTHeader* h = check_get_virt_from_phys((ACPISDTHeader*)xsdt->PointerToOtherSDT[i]);
                if (h != NULL) push_table(h);
            } 

        }
    } 
    
    if (rsdp_desc != NULL) {
        RSDT* rsdt = get_rsdt_from_desc(rsdp_desc);
        if (rsdt == NULL) {
            // panic
            fprintf(stderr, "Warning: could not validade RSDT table %lX\n", (uint64_t)rsdt);      
            exit(2);  
        } else {
            push_table((ACPISDTHeader*)rsdt);

            if (!xsdt_enumerated) {
                int length = rsdt->h.length;
                uint32_t entries = (length - sizeof(rsdt->h))/sizeof(uint32_t);
                for (uint32_t i = 0; i < entries; ++i) {
                    ACPISDTHeader* h = check_get_virt_from_phys((ACPISDTHeader*)(uint64_t)rsdt->PointerToOtherSDT[i]);
                    if (h != NULL) push_table(h);
                } 
            }   
        }
    }

    FADT* fadt_virt = (FADT*)get_table("FACP", 0);
    if (fadt_virt != 0)
        parse_fadt(fadt_virt);

    return 0;
}