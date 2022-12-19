#ifndef INTERRUPTS_H
#define INTERRUPTS_H
#include <stdint.h>

uint8_t get_free_interrupt();

#define INT_MESSAGE_FIXED_VAL 0xFEE
typedef struct Message_Address_Register {
    uint8_t xx : 2;
    uint8_t DM : 1;
    uint8_t RH : 1;
    uint16_t reserved : 8;
    uint16_t dest_id : 8;
    uint16_t int_message_fixed : 12;
} __attribute__((packed)) Message_Address_Register;

typedef struct Message_Data_Register {
    uint8_t vector;
    uint8_t DELMOD : 3;
    uint8_t reserved : 3;
    uint8_t level_trig_mode : 1;
    uint8_t level_trigger : 1;
    uint16_t reserved1;
} __attribute__((packed)) Message_Data_Register;

#endif