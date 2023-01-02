#include <timers/hpet.h>
#include <stddef.h>
#include <stdio.h>
#include <acpi/acpi.h>
#include <phys_map/phys_map.h>
#include <stdio.h>
#include <stdbool.h>
#include <configuration.h>
#include <ioapic/ioapic.h>
#include <pmos/system.h>
#include <timers/timers.h>

static const int hpet_size = 1024;

volatile HPET* hpet_virt = NULL;
unsigned short max_timer = 0;

uint64_t ticks_picos = 0;

void init_hpet()
{
    HPET_Description_Table* hpet_desc_virt = (HPET_Description_Table*)get_table("HPET", 0);
    
    if (hpet_desc_virt == NULL) {
        printf("Info: Did not find valid HPET table...\n");
        return;
    }

    hpet_virt = map_phys((void*)hpet_desc_virt->BASE_ADDRESS.Address, hpet_size);


    HPET_General_Cap_And_Id g = hpet_virt->General_Cap_And_Id; // Accesses must be 32 bit or 64 bit

    if (!g.bits.COUNT_SIZE_CAP) {
        fprintf(stderr, "Warning: HPET counter is 32 bit and that has not yet been implemented\n");
        return;
    }

    max_timer = g.bits.NUM_TIM_CAP;

    ticks_picos = (uint64_t)(1e9)/g.bits.COUNTER_CLK_PERIOD;

    HPET_General_Conf conf = hpet_virt->General_Conf;


    // Reset the timer
    conf.bits.ENABLE_CNF = 0;
    hpet_virt->General_Conf = conf;

    hpet_virt->MAIN_COUNTER_VAL.bits64 = 0;
    conf.bits.ENABLE_CNF = 1;
    hpet_virt->General_Conf = conf;

    // Init timer 0
    // TODO: MSI support
    union HPET_Timer_Conf_cap conf_tmr0 = hpet_virt->timers[0].conf_cap;
    printf("Tn_FSB_INT_DEL_CAP %x Tn_INT_ROUTE_CAP %x\n", conf_tmr0.bits.Tn_FSB_INT_DEL_CAP, conf_tmr0.bits.Tn_INT_ROUTE_CAP);

    // Assuming int 2 is available as it would normally be used for PIT
    // Will probably need redoing

    bool int_2_available = conf_tmr0.bits.Tn_INT_ROUTE_CAP & 0x04;
    if (!int_2_available) {
        fprintf(stderr, "Error: interrupt 2 is not available for IOAPIC. Vector mask: 0x%x\n", conf_tmr0.bits.Tn_INT_ROUTE_CAP);
        return;
    }

    struct int_task_descriptor desc = {getpid(), hpet_int_chan};
    uint8_t int_vec = ioapic_get_int(desc, 2, conf_tmr0.bits.Tn_INT_TYPE_CNF, false);
    if (int_vec == 0) {
        fprintf(stderr, "Error: could not configure IOAPIC\n");
        return;
    } else {
        printf("Info: Assigned INT %i for HPET\n", int_vec);
    }

    conf_tmr0.bits.Tn_INT_ROUTE_CNF = 2;
    conf_tmr0.bits.Tn_INT_ENB_CNF = 1;
    conf_tmr0.bits.Tn_TYPE_CNF = 0;

    hpet_virt->timers[0].conf_cap = conf_tmr0;
}

void hpet_int()
{
    timer_tick();
}

uint64_t hpet_calculate_ticks(uint64_t millis)
{
    return millis*ticks_picos*1000;
}

inline uint64_t hpet_get_ticks()
{
    return hpet_virt->MAIN_COUNTER_VAL.bits64;
}

void hpet_update_system_ticks(uint64_t* system_ticks)
{
    *system_ticks = hpet_get_ticks();
}

void hpet_start_oneshot(uint64_t ticks)
{
    // TODO: This section is critical
    uint64_t current_ticks = hpet_get_ticks();
    uint64_t new_ticks = current_ticks + ticks;

    // TODO: Assuming TMR0 is 64 bits
    hpet_virt->timers[0].comparator.bits64 = new_ticks;
}