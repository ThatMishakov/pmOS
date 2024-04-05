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

#include "sse.hh"
#include <x86_asm.hh>

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