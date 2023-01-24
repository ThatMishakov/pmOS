#include "sse.hh"
#include <asm.hh>

void enable_sse()
{
    u64 cr0 = getCR0();
    cr0 &= ~(0x01UL << 2);  // CR0.EM
    cr0 |=  (0x01   << 1);  // CR0.MP
    cr0 |=  (0x01   << 3);  // CR0.TS (Lazy task switching)
    setCR0(cr0);

    u64 cr4 = getCR4();
    cr4 |=  (0x01   << 9);  // CR4.OSFXSR (enable FXSAVE/FXRSTOR)
    cr4 |=  (0x01   << 10); // CR4.OSXMMEXCPT (enable #XF exception)
    setCR4(cr4);
}

bool sse_is_valid()
{
    return not (getCR0() & (0x01UL << 3));
}

void invalidate_sse()
{
    u64 cr0 = getCR0();
    cr0 |=  (0x01 << 3);  // CR0.TS
    setCR0(cr0);
}

void validate_sse()
{
    u64 cr0 = getCR0();
    cr0 &= ~(0x01 << 3);  // CR0.TS
    setCR0(cr0);
}