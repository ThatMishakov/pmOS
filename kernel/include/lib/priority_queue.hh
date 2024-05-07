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
#include "functional.hh"
#include "vector.hh"

namespace klib
{

template<class T, class Container = vector<T>, class Compare = less<typename Container::value_type>>
class priority_queue
{
private:
    Container C;
    void move_up(size_t i);
    void move_down(size_t i);
    constexpr size_t parent(size_t i) noexcept { return (i - 1) / 2; }
    constexpr size_t left(size_t i) noexcept { return 2 * i + 1; }
    constexpr size_t right(size_t i) noexcept { return 2 * i + 2; }

public:
    typedef Container::value_type value_type;
    typedef Container::const_reference const_reference;
    typedef Container::size_type size_type;

    const_reference top() const;

    [[nodiscard]] bool empty() const { return C.empty(); }

    size_type size() const { return C.size(); }

    void push(const value_type &value);
    void push(value_type &&value);
    void emplace(value_type &&value);

    void pop();
};

template<class T, class Container, class Compare>
void priority_queue<T, Container, Compare>::push(const value_type &value)
{
    this->C.push_back(value);
    move_up(C.size() - 1);
}

template<class T, class Container, class Compare>
void priority_queue<T, Container, Compare>::push(value_type &&value)
{
    this->C.push_back(forward<T>(value));
    move_up(C.size() - 1);
}

template<class T, class Container, class Compare>
void priority_queue<T, Container, Compare>::move_up(size_t i)
{
    while (i > 0) {
        size_t p = parent(i);
        if (Compare()(C[i], C[p])) {
            swap(C[i], C[p]);
        }
        i = p;
    }
}

template<class T, class Container, class Compare>
void priority_queue<T, Container, Compare>::move_down(size_t i)
{
    T tmp    = move(C[i]);
    size_t j = left(i);

    while (j < C.size()) {
        if (j + i < C.size() and Compare()(C[j + i], C[j]))
            ++j;

        if (Compare()(C[j], tmp)) {
            C[i] = move(C[j]);
            i    = j;
            j    = left(i);
        } else {
            break;
        }
    }
    C[i] = move(tmp);
}

template<class T, class Container, class Compare> void priority_queue<T, Container, Compare>::pop()
{
    if (C.size() > 1) {
        C[0] = move(C[C.size() - 1]);
    }
    C.pop_back();

    move_down(0);
}

template<class T, class Container, class Compare>
typename priority_queue<T, Container, Compare>::const_reference
    priority_queue<T, Container, Compare>::top() const
{
    return C[0];
}

} // namespace klib