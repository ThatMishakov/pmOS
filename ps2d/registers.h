#ifndef REGISTERS_H
#define REGISTERS_H

#define DATA_PORT 0x60
#define RW_PORT   0x64

#define CMD_CONFIG_READ     0x20
#define CMD_CONFIG_WRITE    0x60

#define ENABLE_SECOND_PORT  0xA8
#define DISABLE_SECOND_PORT 0xA7
#define TEST_SECOND_PORT    0xA9

#define TEST_CONTROLLER     0xAA
#define TEST_FIRST_PORT     0xAB

#define DIAGNOSTIC_DUMP     0xAC

#define DISABLE_FIRST_PORT  0xAD
#define ENABLE_FIRST_PORT   0xAE

#define READ_OUTPUT         0xD0
#define WRITE_OUTPUT        0xD1

#define PULSE_RESET         0xF0

#define SECOND_PORT         0xD4

#define STATUS_MASK_OUTPUT_FULL 0x01
#define STATUS_MASK_INPUT_FULL  0x02
#define STATUS_MASK_FLAG_SYSTEM 0x03
#define STATUS_MASK_DATA_CMD    0x04
#define STATUS_MASK_TIMEOUT     0x06
#define STATUS_MASK_PARITY_ERR  0x07

#define RESPONSE_ACK            0xFA
#define RESPONSE_FAILURE        0xFC
#define RESPONSE_SELF_TEST_OK   0xAA
#define RESPONSE_ECHO           0xEE

#define COMMAND_ECHO             0xEE
#define COMMAND_IDENTIFY         0xF2
#define COMMAND_ENABLE_SCANNING  0xF4
#define COMMAND_DISABLE_SCANNING 0xF5
#define COMMAND_RESEND           0xFE
#define COMMAND_RESET            0xFF

#endif