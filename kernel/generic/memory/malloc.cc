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

#include "malloc.hh"

#include "palloc.hh"

#include <kern_logger/kern_logger.hh>
#include <lib/new.hh>
#include <utils.hh>

size_t malloced = 0;
size_t freed    = 0;

extern "C" void *mmap(void *, size_t length, int, int, int, long)
{
    // This function is (only) called by malloc
    // Only len is interesting
    void *p = palloc(length / 4096 + (length % 4096 ? 1 : 0));
    return p;
}

extern "C" int munmap(void *addr, size_t length)
{
    (void)addr;
    (void)length;

    serial_logger.printf("Info: munmap(%p, %x)\n", addr, length);

    return -1;
    // Can be implemented easilly, but not supported for now
}

void *operator new(size_t size) { return malloc(size); }

void *operator new[](size_t size) { return malloc(size); }

void operator delete(void *p) noexcept { free(p); }

void operator delete[](void *p) noexcept { free(p); }

void operator delete(void *p, UNUSED size_t s) noexcept { free(p); }

void operator delete[](void *p, UNUSED size_t s) noexcept { free(p); }

void *operator new(size_t, void *p) noexcept { return p; }
void *operator new[](size_t, void *p) noexcept { return p; }
void operator delete(void *, void *) noexcept {};
void operator delete[](void *, void *) noexcept {};

extern "C" int malloc_lock(unsigned int * l)
{
    // This might be UB but it seems to be working...
    Spinlock *s = reinterpret_cast<Spinlock *>(l);
    s->lock();
    return 0;
}

extern "C" void malloc_unlock(unsigned int * l)
{
    Spinlock *s = reinterpret_cast<Spinlock *>(l);
    s->unlock();
}

extern "C" int malloc_try_lock(unsigned int * l)
{
    Spinlock *s = reinterpret_cast<Spinlock *>(l);
    return s->try_lock();
}