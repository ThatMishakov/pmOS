#ifndef COMMANDS_H
#define COMMANDS_H

#define COMMAND_ECHO             0xEE
#define COMMAND_IDENTIFY         0xF2
#define COMMAND_ENABLE_SCANNING  0xF4
#define COMMAND_DISABLE_SCANNING 0xF5
#define COMMAND_RESEND           0xFE
#define COMMAND_RESET            0xFF

#define RESPONSE_ACK            0xFA
#define RESPONSE_FAILURE        0xFC
#define RESPONSE_RESEND         0xFE
#define RESPONSE_SELF_TEST_OK   0xAA
#define RESPONSE_ECHO           0xEE

#endif