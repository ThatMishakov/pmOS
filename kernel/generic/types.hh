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

#pragma once
#include <kernel/types.h>

using kresult_t = u64;

template<class T> struct ReturnStr {
    kresult_t result;
    T val;
};

extern "C" void t_print_bochs(const char *str, ...);

struct Spinlock {
    u32 locked = false;

    void lock() noexcept;
    /// Tries to lock the spinlock. True if lock has been ackquired, false otherwise
    bool try_lock() noexcept;

    // Function to unlock the spinlock
    void unlock() noexcept;

    bool operator==(const Spinlock &s) const noexcept { return this == &s; }

    inline bool is_locked() const noexcept { return locked; }
};

struct Auto_Lock_Scope {
    Spinlock &s;

    Auto_Lock_Scope() = delete;
    Auto_Lock_Scope(Spinlock &lock): s(lock) { s.lock(); }

    ~Auto_Lock_Scope() { s.unlock(); }
};

struct Auto_Lock_Scope_Double {
    Spinlock &a;
    Spinlock &b;
    Auto_Lock_Scope_Double(Spinlock &a, Spinlock &b): a(&a < &b ? a : b), b(&a < &b ? b : a)
    {
        this->a.lock();
        if (a != b)
            this->b.lock();
    }

    ~Auto_Lock_Scope_Double()
    {
        a.unlock();
        if (a != b)
            b.unlock();
    }
};

struct Lock_Guard_Simul {
    Spinlock &a;
    Spinlock &b;

    Lock_Guard_Simul() = delete;
    Lock_Guard_Simul(Spinlock &a, Spinlock &b): a(&a < &b ? a : b), b(&a < &b ? b : a)
    {
        if (a == b) {
            a.lock();
            return;
        }

        while (true) {
            if (not a.try_lock())
                continue;

            if (not b.try_lock()) {
                a.unlock();
                continue;
            }
        }
    }

    ~Lock_Guard_Simul()
    {
        a.unlock();
        if (a != b)
            b.unlock();
    }
};
