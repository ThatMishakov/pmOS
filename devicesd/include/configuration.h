#ifndef CONFIGURATION_H
#define CONFIGURATION_H
#include <stdbool.h>
#include <devicesd/devicesd_msgs.h>

// Returns true if was susccessful
uint8_t configure_interrupts_for(DEVICESD_MESSAGE_REG_INT* desc);

#endif