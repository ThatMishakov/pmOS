#pragma once
#include <pmos/containers/vector.hh>
#include <pmos/containers/string.hh>
#include <cstdint>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <numeric>

namespace pmos::utility
{

class ElFAuxvecBuilder
{
public:
    using data_out_type = pmos::containers::vector<std::byte>;

    struct AuxVecVal {
        int a_type;
        std::variant<long, uintptr_t, pmos::containers::vector<std::byte>> value;
        // 0 - long plain value
        // 1 - pointer or function pointer
        // 2 - free form
    };

    enum class PtrWidth {
        W32bit,
        W64bit,
    };

    pmos::containers::vector<pmos::containers::string> &args();
    const pmos::containers::vector<pmos::containers::string> &args() const;

    pmos::containers::vector<pmos::containers::string> &envp();
    const pmos::containers::vector<pmos::containers::string> &envp() const;

    pmos::containers::vector<AuxVecVal> &auxvec();
    const pmos::containers::vector<AuxVecVal> &auxvec() const;

    // Sets the width for serialization, and returns the previous one
    PtrWidth set_width(PtrWidth width);

    // Size in bytes of the serialized data
    size_t size_serialized() const;

    // Vector containing serialized data (or nothing on error)
    std::optional<data_out_type> serialize(std::uintptr_t stack_end);

    bool is_64bit() const;
protected:
    pmos::containers::vector<pmos::containers::string> args_;
    pmos::containers::vector<pmos::containers::string> envp_;
    pmos::containers::vector<AuxVecVal> auxvec_;
    PtrWidth ptr_width_;

    size_t auxval_size() const;
    size_t strings_size_aligned() const;
};

inline size_t ElFAuxvecBuilder::size_serialized() const
{
    size_t size = 0;

    size_t ptr_size = is_64bit() ? sizeof(uint64_t) : sizeof(uint32_t);

    // Args
    size_t args_count = args_.size() + 1;
    size += (args_count + 1) * ptr_size; // + argument count

    // Envp
    size_t envp_count = envp_.size() + 1;
    size += envp_count * ptr_size;

    // Aux vector entries
    size += auxval_size();

    size += strings_size_aligned();


    // Align to 16 bytes
    size_t mask = 16 - 1;
    size        = (size + mask) & ~mask;

    return size;
}

inline size_t ElFAuxvecBuilder::strings_size_aligned() const
{
    const size_t ptr_size = is_64bit() ? sizeof(uint64_t) : sizeof(uint32_t);
    const size_t ptr_mask = ptr_size - 1;

    size_t size = 0;
    size += std::accumulate(args_.begin(), args_.end(), (size_t)0,
        [](size_t acc, const pmos::containers::string &str) {
            return acc + str.size() + 1;
        }
    );
    size += std::accumulate(envp_.begin(), envp_.end(), (size_t)0,
        [](size_t acc, const pmos::containers::string &str) {
            return acc + str.size() + 1;
        }
    );
    size += std::accumulate(auxvec_.begin(), auxvec_.end(), (size_t)0,
        [=](size_t acc, const auto &vec) {
            size_t aligned_size = 0;
            if (std::holds_alternative<pmos::containers::vector<std::byte>>(vec.value)) {
                const auto &v = std::get<pmos::containers::vector<std::byte>>(vec.value);
                aligned_size = (v.size() + ptr_mask) & ~ptr_mask;
            }
            return acc + aligned_size;
        }
    );

    // Align everything to pointer...
    size = (size + ptr_mask) & ~ptr_mask;
    return size;
}

inline pmos::containers::vector<ElFAuxvecBuilder::AuxVecVal> &ElFAuxvecBuilder::auxvec() { return auxvec_; }
inline pmos::containers::vector<pmos::containers::string> &ElFAuxvecBuilder::args() { return args_; }
inline pmos::containers::vector<pmos::containers::string> &ElFAuxvecBuilder::envp() { return envp_; }
template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };

inline bool ElFAuxvecBuilder::is_64bit() const
{
    return ptr_width_ == PtrWidth::W64bit;
}

inline size_t ElFAuxvecBuilder::auxval_size() const
{
    auto ptr_size = is_64bit() ? sizeof(uint64_t) : sizeof(uint32_t);
    return (auxvec_.size() * 2 + 1) * ptr_size;
}

inline std::optional<ElFAuxvecBuilder::data_out_type> ElFAuxvecBuilder::serialize(std::uintptr_t stack_end)
{
    // Potentially leaves vector messed on error, which is fine here
    auto append_string_bytes = [](pmos::containers::vector<std::byte> &vec, std::string_view s) noexcept -> bool
    {
        auto chars = std::span{s.data(), s.size()};
        auto bytes = std::as_bytes(chars);
        if (vec.append_range(bytes))
            return vec.push_back(std::byte{0});
        else
            return {};
    };

    // Serialize args and environment
    pmos::containers::vector<std::byte> args_serialized;

    pmos::containers::vector<size_t> args_offsets;
    for (const auto &a: args_) {
        auto offset = args_serialized.size();
        if (!args_offsets.push_back(offset))
            return {};

        if (!append_string_bytes(args_serialized, a))
            return {};
    }

    pmos::containers::vector<size_t> environment_offset;
    for (const auto &a: envp_) {
        auto offset = args_serialized.size();
        if (!environment_offset.push_back(offset))
            return {};

        if (!append_string_bytes(args_serialized, a))
            return {};
    }

    size_t alignment_mask = (is_64bit() ? 8 : 4) - 1;
    size_t new_size        = (args_serialized.size() + alignment_mask) & ~alignment_mask;

    if (!args_serialized.resize(new_size, std::byte{0}))
        return {};

    pmos::containers::vector<size_t> extra_offset;
    for (const auto &a: auxvec_) {
        if (!std::holds_alternative<pmos::containers::vector<std::byte>>(a.value))
            continue;

        const auto &v = std::get<pmos::containers::vector<std::byte>>(a.value);

        auto offset = args_serialized.size();
        if (!extra_offset.push_back(offset))
            return {};

        if (!args_serialized.append_range(v))
            return {};

        size_t new_size = (args_serialized.size() + alignment_mask) & ~alignment_mask;

        if (!args_serialized.resize(new_size, std::byte{0}))
            return {};
    }

    size_t final_size = size_serialized();

    pmos::containers::vector<std::byte> output;
    if (!output.reserve(final_size))
        return {};

    uintptr_t start_of_args = stack_end - args_serialized.size();

    // Push arguments, args count and environment
    if (is_64bit()) {
        (void)output.append_range(
            std::bit_cast<std::array<std::byte, sizeof(uint64_t)>>(static_cast<uint64_t>(args_.size())));

        for (auto i: args_offsets) {
            (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(uint64_t)>>(
                static_cast<uint64_t>(i + start_of_args)));
        }

        (void)output.append_range(
            std::bit_cast<std::array<std::byte, sizeof(uint64_t)>>(static_cast<uint64_t>(0)));

        for (auto i: environment_offset) {
            (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(uint64_t)>>(
                static_cast<uint64_t>(i + start_of_args)));
        }

        (void)output.append_range(
            std::bit_cast<std::array<std::byte, sizeof(uint64_t)>>(static_cast<uint64_t>(0)));
    } else {
        (void)output.append_range(
            std::bit_cast<std::array<std::byte, sizeof(uint32_t)>>(static_cast<uint32_t>(args_.size())));

        for (auto i: args_offsets) {
            (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(uint32_t)>>(
                static_cast<uint32_t>(i + start_of_args)));
        }

        (void)output.append_range(
            std::bit_cast<std::array<std::byte, sizeof(uint32_t)>>(static_cast<uint32_t>(0)));

        for (auto i: environment_offset) {
            (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(uint32_t)>>(
                static_cast<uint32_t>(i + start_of_args)));
        }

        (void)output.append_range(
            std::bit_cast<std::array<std::byte, sizeof(uint32_t)>>(static_cast<uint32_t>(0)));
    }

    // Fun part
    if (is_64bit()) {
        struct Serialized {
            uint64_t type;
            uint64_t value;
        };

        size_t current_extra_offset = 0;

        for (const auto& i : auxvec_) {
            uint64_t value = 0;
            if (auto l = std::get_if<long>(&i.value); l) {
                value = static_cast<uint64_t>(*l);
            } else if (auto ptr = std::get_if<uintptr_t>(&i.value); ptr) {
                value = static_cast<uint64_t>(*ptr);
            } else if (auto ii = std::get_if<pmos::containers::vector<std::byte>>(&i.value); ii) {
                auto t = extra_offset.get(current_extra_offset++);
                if (!t)
                    return {};

                value = static_cast<uint64_t>(*t + start_of_args);
            } else {
                assert(false);
            }

            Serialized s{static_cast<uint64_t>(i.a_type), value};

            (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(s)>>(s));
        }

        // Null vector...
        (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(uint64_t)>>((uint64_t)0));
    } else {
        struct Serialized {
            uint32_t type;
            uint32_t value;
        };

        size_t current_extra_offset = 0;

        for (const auto& i : auxvec_) {
            uint32_t value = 0;
            if (auto l = std::get_if<long>(&i.value); l) {
                value = static_cast<uint32_t>(*l);
            } else if (auto ptr = std::get_if<uintptr_t>(&i.value); ptr) {
                value = static_cast<uint32_t>(*ptr);
            } else if (auto ii = std::get_if<pmos::containers::vector<std::byte>>(&i.value); ii) {
                auto t = extra_offset.get(current_extra_offset++);
                if (!t)
                    return {};

                value = static_cast<uint32_t>(*t + start_of_args);
            } else {
                assert(false);
            }

            Serialized s{static_cast<uint32_t>(i.a_type), value};

            (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(s)>>(s));
        }

        // Null vector...
        (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(uint32_t)>>((uint32_t)0));
    }

    size_t to_fill = final_size - output.size() - args_serialized.size();
    assert(to_fill <= 12);
    
    (void)output.insert(output.end(), to_fill, std::byte{0});

    (void)output.append_range(std::move(args_serialized));

    return output;
}

inline ElFAuxvecBuilder::PtrWidth ElFAuxvecBuilder::set_width(ElFAuxvecBuilder::PtrWidth width)
{
    std::swap(ptr_width_, width);
    return width;
}

} // namespace pmos::utility