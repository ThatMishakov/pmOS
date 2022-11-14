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

