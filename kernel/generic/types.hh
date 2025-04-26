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
#include <array>
#include <assert.h>
#include <concepts>
#include <kernel/types.h>
#include <lib/utility.hh>

using kresult_t = i64;

struct Error {
    kresult_t result {0};

    Error() = default;
    inline Error(kresult_t result): result(result) {};
};

template<class T> struct ReturnStr {
    kresult_t result;
    T val;

    ReturnStr() = default;
    ReturnStr(kresult_t result, T val): result(result), val(klib::move(val)) {};
    ReturnStr(Error e): result {e.result}, val {} {};
    ReturnStr(T val): result {0}, val(klib::move(val)) {};

    template<class U> ReturnStr(ReturnStr<U> u): result {u.result}, val(std::move(u.val)) {};

    Error propagate() const
    {
        assert(result);
        return {result};
    }

    bool success() const { return result == 0; }

    static ReturnStr error(kresult_t result)
    {
        assert(result != 0);
        return {result, {}};
    }

    // TODO: move this...
    static ReturnStr success(T val) { return {0, klib::move(val)}; }
};

template<typename T> ReturnStr<T> Success(T r) { return ReturnStr<T>::success(std::move(r)); }

extern "C" void t_print_bochs(const char *str, ...);

class Spinlock_base
{
private:
    u32 locked = 0;

protected:
    void lock() noexcept;
    bool try_lock() noexcept;
    void unlock() noexcept;

public:
    bool operator==(const Spinlock_base &s) const noexcept { return this == &s; }
    inline bool is_locked() const noexcept { return locked; }
};

class Spinlock: public Spinlock_base
{
public:
    void lock() noexcept;

    /// Tries to lock the spinlock. True if lock has been ackquired, false otherwise
    bool try_lock() noexcept;

    // Function to unlock the spinlock
    void unlock() noexcept { Spinlock_base::unlock(); }
};

class CriticalSpinlock: public Spinlock_base
{
public:
    inline void lock() noexcept { Spinlock_base::lock(); }
    /// Tries to lock the spinlock. True if lock has been ackquired, false otherwise
    inline bool try_lock() noexcept { return Spinlock_base::try_lock(); }

    // Function to unlock the spinlock
    inline void unlock() noexcept { Spinlock_base::unlock(); }
};

template<typename T>
concept spinlock_type = requires(T s) {
    { s.lock() } -> std::same_as<void>;
    // { s.try_lock() } -> std::same_as<bool>;
    { s.unlock() } -> std::same_as<void>;
};

template<spinlock_type T> struct Auto_Lock_Scope {
    T &s;

    Auto_Lock_Scope() = delete;
    Auto_Lock_Scope(T &lock): s(lock) { s.lock(); }

    ~Auto_Lock_Scope() { s.unlock(); }
};

template<spinlock_type T> struct Auto_Lock_Scope_Double {
    T &a;
    T &b;
    Auto_Lock_Scope_Double(T &a, T &b): a(&a < &b ? a : b), b(&a < &b ? b : a)
    {
        this->a.lock();
        if (&a != &b)
            this->b.lock();
    }

    ~Auto_Lock_Scope_Double()
    {
        a.unlock();
        if (&a != &b)
            b.unlock();
    }
};

template<spinlock_type T> struct Lock_Guard_Simul {
    T &a;
    T &b;

    Lock_Guard_Simul() = delete;
    Lock_Guard_Simul(T &a, T &b): a(&a < &b ? a : b), b(&a < &b ? b : a)
    {
        if (&a == &b) {
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
        if (&a != &b)
            b.unlock();
    }
};

template<spinlock_type T, size_t max_locks> struct LockCarousel {
    std::array<T *, max_locks> locks;
    size_t count = 0;

    bool insert(T &lock)
    {
        T *ptr   = &lock;
        size_t i = 0;
        for (; i < count; ++i) {
            if (locks[i] == ptr)
                return false;
            if (locks[i] > ptr)
                break;
        }
        assert(count < max_locks);
        for (size_t j = count; j > i; --j)
            locks[j] = locks[j - 1];
        locks[i] = ptr;
        ++count;
        return true;
    }

    void reset() { count = 0; }

    void lock()
    {
        for (size_t i = 0; i < count; ++i)
            locks[i]->lock();
    }

    void unlock()
    {
        for (size_t i = count; i > 0; --i) {
            locks[i - 1]->unlock();
        }
    }
};

inline void spin_pause() {}

inline constexpr auto PAGE_ORDER = 12;
// inline constexpr auto PAGE_SIZE  = 4096;
#define PAGE_SIZE 4096

inline constexpr auto MAX_PHYS_ADDR_ORDER = 56;

namespace kernel::types
{
inline int log2(u64 n) noexcept { return sizeof(n) * 8 - __builtin_clzll(n) - 1; }
} // namespace kernel::types