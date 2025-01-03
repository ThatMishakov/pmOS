/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "sse.hh"

#include <kern_logger/kern_logger.hh>
#include <stdlib.h>
#include <x86_asm.hh>
#include <x86_utils.hh>

static constexpr u64 CR4_OSXSAVE = 1 << 18;

static SSECtxStyle sse_ctx_style = SSECtxStyle::FXSAVE;
static size_t sse_ctx_size       = 512;
static u64 xcr0                  = 0;
static bool xsave_supported      = false;

void detect_sse()
{
    auto t          = cpuid(0x01);
    xsave_supported = t.ecx & (0x01 << 26);

    if (xsave_supported) {
        setCR4(getCR4() | CR4_OSXSAVE);
        sse_ctx_style = SSECtxStyle::XSAVE;

        // set xcr0
        t    = cpuid2(0x0D, 0);
        xcr0 = ((u64)t.edx << 32) | t.eax;
        set_xcr(0, xcr0);

        // get size of XSAVE area
        sse_ctx_size = t.ecx;

        // Detect XSAVEOPT support
        t = cpuid2(0x0D, 1);
        if (t.eax & 0x01)
            sse_ctx_style = SSECtxStyle::XSAVEOPT;
    }

    serial_logger.printf(
        "SSE: XSAVE %s supported, XSAVEOPT %s supported, XSAVE area size: %u bytes\n",
        xsave_supported ? "" : "not", sse_ctx_style == SSECtxStyle::XSAVEOPT ? "" : "not",
        sse_ctx_size);

    if (sse_ctx_size < 512) {
        serial_logger.printf("SSE: XSAVE area size is too small, panicking\n");
        abort();
    }
}

void enable_sse()
{
    u64 cr0 = getCR0();
    cr0 &= ~(0x01UL << 2); // CR0.EM
    cr0 |= (0x01 << 1);    // CR0.MP
    cr0 |= (0x01 << 3);    // CR0.TS (Lazy task switching)
    setCR0(cr0);

    u64 cr4 = getCR4();
    cr4 |= (0x01 << 9);  // CR4.OSFXSR (enable FXSAVE/FXRSTOR)
    cr4 |= (0x01 << 10); // CR4.OSXMMEXCPT (enable #XF exception)
    setCR4(cr4);

    if (xsave_supported) {
        setCR4(getCR4() | CR4_OSXSAVE);
        set_xcr(0, xcr0);
    }
}

bool sse_is_valid() { return not(getCR0() & (0x01UL << 3)); }

void invalidate_sse()
{
    u64 cr0 = getCR0();
    cr0 |= (0x01 << 3); // CR0.TS
    setCR0(cr0);
}

void validate_sse()
{
    u64 cr0 = getCR0();
    cr0 &= ~(0x01 << 3); // CR0.TS
    setCR0(cr0);
}

// TODO: move this to a better place
extern "C" void *memalign(size_t alignment, size_t size);

kresult_t SSE_Data::init_on_thread_start()
{
    data = klib::unique_ptr<u8>((u8 *)memalign(64, sse_ctx_size));
    if (!data)
        return -ENOMEM;

    // Set the initial state
    memset(data.get(), 0, sse_ctx_size);

    // set MXCSR to default value
    *(u32 *)(data.get() + 24) = 0x1F80;

    return 0;
}

void SSE_Data::save_sse()
{
    switch (sse_ctx_style) {
    case SSECtxStyle::FXSAVE:
        asm("fxsaveq (%0)" : : "r"(data.get()));
        break;
    case SSECtxStyle::XSAVE:
        asm("xsaveq (%0)" : : "r"(data.get()), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF));
        break;
    case SSECtxStyle::XSAVEOPT:
        asm("xsaveoptq (%0)" : : "r"(data.get()), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF));
        break;
    default:
        assert(false);
    }
}

void SSE_Data::restore_sse()
{
    switch (sse_ctx_style) {
    case SSECtxStyle::FXSAVE:
        asm("fxrstorq (%0)" : : "r"(data.get()));
        break;
    case SSECtxStyle::XSAVE:
    case SSECtxStyle::XSAVEOPT:
        asm("xrstorq (%0)" : : "r"(data.get()), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF));
        break;
    default:
        assert(false);
    }
}