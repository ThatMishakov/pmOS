#pragma once

#include "../utility/bitops.hh"
#include <stddef.h>
#include <stdint.h>
#include <span>
#include <algorithm>

namespace pmos::containers
{

template<size_t Capacity>
struct byte_ring_buffer {
    static_assert(pmos::utility::is_p2(Capacity), "Capacity must be a power of 2");

    [[nodiscard]] constexpr bool empty() const { return tail_ == head_; }
    [[nodiscard]] constexpr size_t size() const { return head_ - tail_; }
    [[nodiscard]] constexpr size_t capacity() const { return Capacity; }
    [[nodiscard]] constexpr size_t available_space() const { return capacity() - size(); }
    constexpr void clear() { head_ = tail_; }

    [[nodiscard]] size_t enqueue(std::span<const uint8_t> data) {
        const size_t to_write = std::min(data.size(), available_space());
        const size_t head_index = head_ & mask_;
        const size_t bytes_until_wrap = capacity() - head_index;

        if (to_write <= bytes_until_wrap) {
            std::copy_n(data.begin(), to_write, buffer + head_index);
        } else {
            std::copy_n(data.begin(), bytes_until_wrap, buffer + head_index);
            std::copy_n(data.begin() + bytes_until_wrap, to_write - bytes_until_wrap, buffer);
        }

        head_ += to_write;
        return to_write;
    }

    size_t peek(std::span<uint8_t> out) const {
        const size_t to_read = std::min(out.size(), size());
        const size_t tail_index = tail_ & mask_;
        const size_t bytes_until_wrap = capacity() - tail_index;

        if (to_read <= bytes_until_wrap) {
            std::copy_n(buffer + tail_index, to_read, out.begin());
        } else {
            std::copy_n(buffer + tail_index, bytes_until_wrap, out.begin());
            std::copy_n(buffer, to_read - bytes_until_wrap, out.begin() + bytes_until_wrap);
        }

        return to_read;
    }

    size_t dequeue(std::span<uint8_t> out) {
        const size_t to_read = peek(out);
        tail_ += to_read;
        return to_read;
    }

    constexpr size_t pop_bytes(size_t count) {
        const size_t to_pop = std::min(count, size());
        tail_ += to_pop;
        return to_pop;
    }
private:
    uint8_t buffer[Capacity];
    size_t head_ = 0;
    size_t tail_ = 0;
    static constexpr size_t mask_ = Capacity - 1;
};

} // namespace pmos::containers