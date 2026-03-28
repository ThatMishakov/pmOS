/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <acpi/acpi.h>
#include <panic.h>
#include <phys_map/phys_map.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

extern uint64_t rsdp_desc;

typedef struct ACPITreeNode {
    ACPISDTHeader *virt_addr;
    struct ACPITreeNode *left;
    struct ACPITreeNode *right;
} ACPITreeNode;

static ACPITreeNode *root;

ACPISDTHeader *get_table(const char *signature, int n)
{
    if (!root)
        return NULL;

    ACPITreeNode *ptr = root;
    ACPITreeNode *t   = NULL;

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

void push_table(ACPISDTHeader *acpi_table_virt)
{
    ACPITreeNode *node = malloc(sizeof(ACPITreeNode));
    if (node == NULL) {
        // panic("Error: Could not allocate memory\n");
        exit(2);
    }

    node->left      = NULL;
    node->right     = NULL;
    node->virt_addr = acpi_table_virt;

    // printf("Adding table %.4s\n", acpi_table_virt->signature);

    if (!root) {
        root = node;
    } else {
        ACPITreeNode *ptr = root;
        int cmp           = strncmp(ptr->virt_addr->signature, acpi_table_virt->signature, 4);

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
ACPISDTHeader *check_get_virt_from_phys(uint64_t phys)
{
    ACPISDTHeader *virt_small = map_phys(phys, sizeof(ACPISDTHeader *));
    uint32_t size             = virt_small->length;
    unmap_phys(virt_small, sizeof(ACPISDTHeader *));

    // TODO: I don't know what is a sane value as I've seen some *huge* tables on my laptop
    // // Check if size is sane
    // if (size > 0x1000000)
    //     return NULL;

    ACPISDTHeader *virt_big = map_phys(phys, size);
    // Wisdom from the internet: ACPI checksum is broken half the time and nobody checks it
    // if (check_table(virt_big)) {
    //     return virt_big;
    // }
    return virt_big;

    // On error
    unmap_phys(virt_big, size);
    return NULL;
}

RSDT *get_rsdt_from_desc(uint64_t rsdp_desc_phys)
{
    RSDP_descriptor *rsdp_virt = map_phys(rsdp_desc_phys, sizeof(RSDP_descriptor));
    if (rsdp_virt == NULL) {
        fprintf(stderr, "Warning: Could not map rsdp\n");
        return NULL;
    }

    unsigned char sum = 0;
    for (size_t i = 0; i < sizeof(RSDP_descriptor); ++i)
        sum += ((unsigned char *)rsdp_virt)[i];

    if (sum || strncmp("RSD PTR ", rsdp_virt->signature, 8)) {
        fprintf(stderr, "Warning: RSDP Descriptor not valid\n");
        return NULL;
    }


    RSDT *rsdt_virt = (RSDT *)check_get_virt_from_phys((uint64_t)rsdp_virt->rsdt_address);

    if (!rsdt_virt || strncmp("RSDT", rsdt_virt->h.signature, 4)) {
        fprintf(stderr, "Warning: RSDT pointed by RSDP not valid\n");
        return NULL;
    }

    return rsdt_virt;
}

XSDT *get_xsdt_from_desc(uint64_t rsdp_desc_phys)
{
    RSDP_descriptor20 *rsdp_virt = map_phys(rsdp_desc_phys, sizeof(RSDP_descriptor20));

    if (rsdp_virt == NULL) {
        // panic("Could not map rsdp\n");
    }

    if (rsdp_virt->firstPart.revision < 2) { // Not RDSP 2.0 pointer
        return NULL;
    }

    unsigned char sum = 0;
    for (size_t i = 0; i < sizeof(RSDP_descriptor20); ++i)
        sum += ((unsigned char *)rsdp_virt)[i];

    if (sum || strncmp("RSD PTR ", rsdp_virt->firstPart.signature, 8)) {
        fprintf(stderr, "Warning: RSDP Descriptor not valid\n");
        return NULL;
    }

    if (rsdp_virt->firstPart.revision < 2)
        return NULL; // ACPI ver. 1

    XSDT *xsdt_virt = (XSDT *)check_get_virt_from_phys((uint64_t)
            rsdp_virt->xsdt_address);

    if (!xsdt_virt || strncmp("XSDT", xsdt_virt->h.signature, 4)) {
        fprintf(stderr, "Warning: XSDT pointed by RSDP not valid\n");
        return NULL;
    }

    return xsdt_virt;
}

void parse_fadt(FADT *fadt_virt)
{
    uint8_t revision = fadt_virt->h.revision;

    // TODO: FACS
    uint64_t dsdt_phys = 0;

    if (revision < 2) {
        dsdt_phys = (uint64_t)fadt_virt->dsdt_phys;
    } else {
        dsdt_phys = fadt_virt->X_DSDT;
    }

    DSDT *dsdt_virt = (DSDT *)check_get_virt_from_phys(dsdt_phys);
    if (dsdt_virt == NULL || strncmp(dsdt_virt->h.signature, "DSDT", 4)) {
        fprintf(stderr,
                "Panic: Weird DSDT table pointer in FADT table, dsdt_phys %" PRIx64 " dsdt_virt: %p "
                "revision: %i\n",
                (uint64_t)dsdt_phys, dsdt_virt, revision);
        exit(2);
    }

    push_table((ACPISDTHeader *)dsdt_virt);
}

// Returns -1 on error or ACPI version otherwise
int walk_acpi_tables()
{
    int revision         = -1;
    char xsdt_enumerated = 0;
    if (rsdp_desc != 0) { // TODO
        XSDT *xsdt = get_xsdt_from_desc(rsdp_desc);
        if (xsdt == NULL) {
            fprintf(stderr, "Warning: could not validade XSDT table %" PRIx64 "\n", (uint64_t)xsdt);
        } else {
            xsdt_enumerated = 1;
            push_table((ACPISDTHeader *)xsdt);
            int length       = xsdt->h.length;
            uint32_t entries = (length - sizeof(xsdt->h)) / sizeof(uint64_t);
            for (uint32_t i = 0; i < entries; ++i) {
                ACPISDTHeader *h =
                    check_get_virt_from_phys(xsdt->PointerToOtherSDT[i]);
                if (h != NULL)
                    push_table(h);
            }
        }
    }

    if (rsdp_desc != 0) {
        RSDT *rsdt = get_rsdt_from_desc(rsdp_desc);
        if (rsdt == NULL) {
            // panic
            fprintf(stderr, "Warning: could not validade RSDT table %" PRIx64 "\n", (uint64_t)rsdt);
            exit(2);
        } else {
            push_table((ACPISDTHeader *)rsdt);

            if (!xsdt_enumerated) {
                int length       = rsdt->h.length;
                uint32_t entries = (length - sizeof(rsdt->h)) / sizeof(uint32_t);
                for (uint32_t i = 0; i < entries; ++i) {
                    ACPISDTHeader *h = check_get_virt_from_phys(
                        (uint64_t)rsdt->PointerToOtherSDT[i]);
                    if (h != NULL)
                        push_table(h);
                }
            }
        }
    }

    FADT *fadt_virt = (FADT *)get_table("FACP", 0);
    if (fadt_virt != 0) {
        revision = fadt_virt->h.revision;
        parse_fadt(fadt_virt);
    }

    return revision;
}