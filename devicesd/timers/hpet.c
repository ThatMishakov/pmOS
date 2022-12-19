#include <timers/hpet.h>
#include <stddef.h>
#include <stdio.h>
#include <acpi/acpi.h>
#include <phys_map/phys_map.h>

static const int hpet_size = 1024;

volatile HPET* hpet_virt = NULL;
short max_timer = 0;

void init_hpet()
{
    HPET_Description_Table* hpet_desc_virt = (HPET_Description_Table*)get_table("HPET", 0);
    
    if (hpet_desc_virt == NULL) {
        printf("Info: Did not find valid HPET table...\n");
        return;
    }

    hpet_virt = map_phys((void*)hpet_desc_virt->BASE_ADDRESS.Address, hpet_size);


    HPET_General_Cap_And_Id g = hpet_virt->General_Cap_And_Id; // Accesses must be 32 bit or 64 bit
    max_timer = g.bits.NUM_TIM_CAP;

    
}