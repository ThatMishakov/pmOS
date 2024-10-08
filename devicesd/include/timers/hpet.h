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

#ifndef HPET_H
#define HPET_H
#include <interrupts/interrupts.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct HPET_TIMER {
    union HPET_Timer_Conf_cap {
        struct {
            uint8_t Reserved : 1;
            uint8_t INT_TYPE_CNF : 1;
            uint8_t INT_ENB_CNF : 1;
            uint8_t TYPE_CNF : 1;
            uint8_t PER_INT_CAP : 1;
            uint8_t SIZE_CAP : 1;
            uint8_t VAL_SET_CNF : 1;
            uint8_t Reserved1 : 1;
            uint8_t _32MODE_CNF : 1;
            uint8_t INT_ROUTE_CNF : 4;
            uint8_t FSB_EN_CNF : 1;
            uint8_t FSB_INT_DEL_CAP : 1;
            uint16_t Reserved2;
            uint32_t INT_ROUTE_CAP;
        } __attribute__((packed)) bits;
        uint64_t aslong;
    } __attribute__((packed)) conf_cap;

    union {
        uint64_t bits64;
        uint32_t bits32;
    } __attribute__((packed)) comparator;

    struct {
        union {
            uint32_t asint;
            Message_Data_Register bits;
        } __attribute__((packed)) FSB_INT_VAL;
        union {
            uint32_t asint;
            Message_Address_Register bits;
        } __attribute__((packed)) FSB_INT_ADDR;
    } __attribute__((packed)) fsb_route_reg;

    uint64_t reserved;
} __attribute__((packed)) HPET_TIMER;

typedef union {
    uint64_t aslong;
    struct {
        uint8_t REV_ID;
        uint8_t NUM_TIM_CAP : 5;
        uint8_t COUNT_SIZE_CAP : 1;
        uint8_t Reserved : 1;
        uint8_t LEG_RT_CAP : 1;
        uint16_t VENDOR_ID;
        uint32_t COUNTER_CLK_PERIOD;
    } __attribute__((packed)) bits;
} __attribute__((packed)) HPET_General_Cap_And_Id;

typedef union {
    uint64_t aslong;
    struct {
        uint8_t ENABLE_CNF : 1;
        uint8_t LEG_RT_CNF : 1;
        uint8_t reserved0 : 6;
        uint8_t reserved_non_os;
        uint64_t reserved2 : 48;
    } bits;
} __attribute__((packed)) HPET_General_Conf;

typedef union {
    uint64_t aslong;
    struct {
        uint8_t T0_INT_STS : 1;
        uint8_t T1_INT_STS : 1;
        uint8_t T2_INT_STS : 1;
        uint32_t INT_STS : 29;
        uint32_t reserved;
    } bits;
} __attribute__((packed)) HPET_General_Int_Status;

typedef struct HPET {

    HPET_General_Cap_And_Id General_Cap_And_Id;
    uint64_t reserved0;
    HPET_General_Conf General_Conf;
    uint64_t reserved1;
    HPET_General_Int_Status General_Int_Status;
    uint64_t reserved2[25];

    union {
        uint64_t bits64;
        uint32_t bits32;
    } __attribute__((packed)) MAIN_COUNTER_VAL;

    uint64_t reserved3;

    HPET_TIMER timers[0];
} __attribute__((packed)) HPET;

extern volatile HPET *hpet_virt;
extern bool hpet_is_functional;
extern unsigned short max_timer;
extern uint32_t hpet_clock_period;
extern uint64_t hpet_frequency;
extern uint64_t hpet_ticks_per_ms;

int hpet_init_int_msi(volatile HPET_TIMER *hpet_virt);
int hpet_init_int_ioapic(volatile HPET_TIMER *hpet_virt);
int init_hpet();
void hpet_int();

uint64_t hpet_calculate_ticks(uint64_t millis);
extern uint64_t hpet_get_ticks();

void hpet_update_system_ticks(uint64_t *system_ticks);
void hpet_start_oneshot(uint64_t ticks);

void hpet_program_periodic(uint64_t ticks);

#define HPET_TMR0_COMPARATOR_OFFSET 0x108
void hpet_write_register(uint64_t position, uint64_t value);

void hpet_stop_counter();
void hpet_start_counter();

#endif