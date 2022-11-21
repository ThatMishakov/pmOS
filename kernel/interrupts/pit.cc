#include "pit.hh"
#include <utils.hh>

u16 read_pit_count(u8 chan)
{
    PIT_Cmd c = {0, 0, 0, chan};

    outb(PIT_MODE_REG, *(u8*)&c);

    u16 count;
    count = inb(PIT_CH0_DATA+chan);
    count |= inb(PIT_CH0_DATA+chan) << 8;
    return count;
}

void set_pit_count(u16 count, u8 chan)
{
    outb(PIT_CH0_DATA+chan, count&0xff);
    outb(PIT_CH0_DATA+chan, count >> 8);
}

void pit_sleep_100us(u16 time)
{
    outb(PIT_MODE_REG, 0b10'11'001'0);
    set_pit_count(119*time, 2);

    // Start timer 2
    u8 p = inb(0x61);
    p = (p&0xfe); 
    outb(0x61, p); // Gate LOW
    outb(0x61, p | 1); // Gate HIGH

    while (not (inb(0x61)&0x20));
}

