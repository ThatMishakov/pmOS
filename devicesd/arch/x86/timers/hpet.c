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
#include <asm.h>
#include <main.h>

static const int hpet_size = 1024;

volatile HPET* hpet_virt = NULL;
bool hpet_is_functional = false;
unsigned short max_timer = 0;
uint32_t hpet_clock_period = 0;
uint64_t hpet_frequency = 0;
uint64_t hpet_ticks_per_ms = 0;

int hpet_init_int_msi(volatile HPET_TIMER* timer)
{
    // TODO
    return 1;
}

int hpet_init_int_ioapic(volatile HPET_TIMER* timer)
{
    union HPET_Timer_Conf_cap conf_tmr0 = timer->conf_cap;

    conf_tmr0.bits.INT_TYPE_CNF = 0;

    unsigned ioapic_input = 0;
    unsigned int_vec = 0;
    struct int_task_descriptor desc = {getpid(), main_port};

    for (int i = 23; i >= 0; --i) {
        if (conf_tmr0.bits.INT_ROUTE_CAP & (0x01 << i)) {
            int_vec = ioapic_get_int(desc, 2, false, conf_tmr0.bits.INT_TYPE_CNF, true);
            if (int_vec != 0) {
                ioapic_input = i;
                break;
            }
        }
    }
    
    if (int_vec == 0) {
        fprintf(stderr, "Error: could not configure IOAPIC\n");
        return 1;
    } else {
        printf("Info: Assigned HPET interrupt IOAPIC %i -> %i\n", ioapic_input, int_vec);
    }

    conf_tmr0.bits.INT_ROUTE_CNF = ioapic_input;
    conf_tmr0.bits.INT_TYPE_CNF = 0;
    conf_tmr0.bits.INT_ENB_CNF = 0;

    timer->conf_cap = conf_tmr0;

    return 0;
}

int init_hpet()
{
    HPET_Description_Table* hpet_desc_virt = (HPET_Description_Table*)get_table("HPET", 0);
    
    if (hpet_desc_virt == NULL) {
        printf("Info: Did not find valid HPET table...\n");
        return 1;
    }

    hpet_virt = map_phys((void*)hpet_desc_virt->BASE_ADDRESS.Address, hpet_size);


    HPET_General_Cap_And_Id g = hpet_virt->General_Cap_And_Id; // Accesses must be 32 bit or 64 bit

    // if (!g.bits.COUNT_SIZE_CAP) {
    //    fprintf(stderr, "Warning: HPET counter is 32 bit and that has not yet been implemented\n");
    //    return 1;
    // }

    max_timer = g.bits.NUM_TIM_CAP;

    hpet_clock_period = g.bits.COUNTER_CLK_PERIOD;
    hpet_frequency = (uint64_t)(1e15)/hpet_clock_period;
    hpet_ticks_per_ms = hpet_frequency/1000;

    HPET_General_Conf conf = hpet_virt->General_Conf;


    // Reset the timer
    conf.bits.ENABLE_CNF = 0;
    hpet_virt->General_Conf = conf;

    hpet_virt->MAIN_COUNTER_VAL.bits64 = 0;

    // Init timer 0
    // TODO: MSI support
    union HPET_Timer_Conf_cap conf_tmr0 = hpet_virt->timers[0].conf_cap;
    printf("FSB_INT_DEL_CAP %x INT_ROUTE_CAP %x\n", conf_tmr0.bits.FSB_INT_DEL_CAP, conf_tmr0.bits.INT_ROUTE_CAP);


    do {
        int result;

        result = hpet_init_int_msi(&hpet_virt->timers[0]);
        if (result == 0) break;

        result = hpet_init_int_ioapic(&hpet_virt->timers[0]);
        if (result == 0) break;

        printf("Warning: Could not init HPET interrupts\n");
        return 1;
    } while (false);

    conf_tmr0 = hpet_virt->timers[0].conf_cap;
    conf_tmr0.bits.TYPE_CNF = 0;
    hpet_virt->timers[0].conf_cap = conf_tmr0;

    conf_tmr0.bits.INT_ENB_CNF = 1;
    hpet_virt->timers[0].comparator.bits64 = ~0x0UL;
    hpet_virt->timers[0].conf_cap = conf_tmr0;

    conf.bits.ENABLE_CNF = 0;
    hpet_virt->General_Conf = conf;

    hpet_is_functional = true;

    printf("Initialized HPET at %lx. Frequency: %liHz\n", hpet_desc_virt->BASE_ADDRESS.Address, hpet_frequency);
    return 0;
}

void hpet_int()
{
    timer_tick();
}

uint64_t hpet_calculate_ticks(uint64_t millis)
{
    return millis*hpet_ticks_per_ms;
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
    if (ticks < hpet_clock_period) {
        ticks = hpet_clock_period;
    }

    // TODO: This section is critical
    uint64_t current_ticks = hpet_get_ticks();
    uint64_t new_ticks = current_ticks + ticks;

    // TODO: Assuming TMR0 is 64 bits
    hpet_virt->timers[0].comparator.bits64 = new_ticks;
    uint64_t comparator = hpet_virt->timers[0].comparator.bits64;

    printf("HPET oneshot %lx status reg %lx comparator %lx\n", new_ticks, hpet_virt->timers[0].conf_cap.aslong, comparator);
}

void hpet_program_periodic(uint64_t ticks)
{
    hpet_start_counter();

    printf("Programming HPET for interrupt every %lx ticks...\n", ticks);

    union HPET_Timer_Conf_cap conf_tmr0 = hpet_virt->timers[0].conf_cap;

    conf_tmr0.bits.INT_ENB_CNF = 1;
    conf_tmr0.bits._32MODE_CNF = 1;
    conf_tmr0.bits.TYPE_CNF = 1; // Periodic mode
    conf_tmr0.bits.VAL_SET_CNF = 1; // SET_CNF

    hpet_virt->timers[0].conf_cap = conf_tmr0;

    uint64_t cmp = hpet_get_ticks() + ticks;

    hpet_write_register(HPET_TMR0_COMPARATOR_OFFSET, cmp); 
    io_wait();
    hpet_write_register(HPET_TMR0_COMPARATOR_OFFSET, ticks);

    hpet_start_counter();
}

void hpet_write_register(uint64_t position, uint64_t value)
{
    volatile uint64_t *hpet_register = (uint64_t*)((char*)hpet_virt + position);

    *hpet_register = value;
}

void hpet_stop_counter()
{
    HPET_General_Conf conf = hpet_virt->General_Conf;
    conf.bits.ENABLE_CNF = 0;
    hpet_virt->General_Conf = conf;
}

void hpet_start_counter()
{
    HPET_General_Conf conf = hpet_virt->General_Conf;
    conf.bits.ENABLE_CNF = 1;
    hpet_virt->General_Conf = conf;
}