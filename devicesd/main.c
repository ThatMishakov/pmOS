#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
#include <string.h>
#include <system.h>
#include <stdio.h>
#include "acpi.h"
#include <stddef.h>

char* exec = NULL;

void usage()
{
    printf("Usage: %s [args] - initializes and manages devices in pmOS.\n"
            " Supported arguments:\n"
            "   --rsdp-desc <addr> - sets RSDP physical address\n"
            "   --rsdp20-desc <addr> - sets RSDP 2.0 physical address\n", exec ? exec : "");
    exit(0);
}

void parse_args(int argc, char** argv)
{
    if (argc < 1) return;
    exec = argv[0];

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--rsdp-desc") == 0) {
            ++i;
            if (i == argc) {
                printf("Error: malformed options\n");
                usage();
            }
            rsdp_desc = (RSDP_descriptor*)strtoul(argv[i], NULL, 0);
        } else if (strcmp(argv[i], "--rsdp20-desc") == 0) {
            ++i;
            if (i == argc) {
                printf("Error: malformed options\n");
                usage();
            }
            rsdp20_desc = (RSDP_descriptor20*)strtoul(argv[i], NULL, 0);
        }
    }
}

int main(int argc, char** argv) {
    puts("Hello from devicesd!\n");
    
    parse_args(argc, argv);

    if (rsdp_desc || rsdp20_desc)
        init_acpi();

    return 0;
}