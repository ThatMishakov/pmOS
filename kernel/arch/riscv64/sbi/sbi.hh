#pragma once

struct sbiret {
    long error;
    long value;
};

extern "C" struct sbiret sbi_debug_console_write_byte(unsigned char byte);

struct sbiret sbi_send_ipi(unsigned long hart_mask,
                           unsigned long hart_mask_base);

extern "C" struct sbiret sbi_set_timer(unsigned long stime_value);