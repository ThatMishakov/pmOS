#include "acpi.h"
#include <stdio.h>

RSDP_descriptor* rsdp_desc = NULL;
RSDP_descriptor20* rsdp20_desc = NULL;

void init_acpi()
{
    printf("Info: Initializing ACPI...\n");
}