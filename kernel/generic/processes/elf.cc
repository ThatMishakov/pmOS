#include "elf.hh"
#include <optional>
#include <numeric>

namespace kernel::proc
{

size_t ElFAuxvec::size_serialized() const
{
    size_t size = 0;

    size_t ptr_size = is_64bit() ? sizeof(u64) : sizeof(u32);

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

size_t ElFAuxvec::strings_size_aligned() const
{
    const size_t ptr_size = is_64bit() ? sizeof(u64) : sizeof(u32);
    const size_t ptr_mask = ptr_size - 1;

    size_t size = 0;
    size += std::accumulate(args_.begin(), args_.end(), 0,
        [](size_t acc, const klib::string &str) {
            return acc + str.size() + 1;
        }
    );
    size += std::accumulate(envp_.begin(), envp_.end(), 0,
        [](size_t acc, const klib::string &str) {
            return acc + str.size() + 1;
        }
    );
    size += std::accumulate(aux_.begin(), aux_.end(), 0,
        [=](size_t acc, const auto &vec) {
            auto size = vec.size();
            auto aligned_size = (size + ptr_mask) & ~ptr_mask;
            return acc + aligned_size;
        }
    );

    // Align everything to pointer...
    size = (size + ptr_mask) & ~ptr_mask;
    return size;
}

klib::vector<ElFAuxvec::AuxVecVal> &ElFAuxvec::auxvec() { return auxvec_; }

klib::vector<klib::vector<std::byte>> &ElFAuxvec::extra_info() { return aux_; }

klib::vector<klib::string> &ElFAuxvec::args() { return args_; }

// Potentially leaves vector messed on error. Which is fine where I use it now
static bool append_string_bytes(klib::vector<std::byte> &vec, std::string_view s) noexcept
{
    auto chars = std::span{s.data(), s.size()};
    auto bytes = std::as_bytes(chars);
    if (vec.append_range(bytes))
        return vec.push_back(std::byte{0});
    else
        return {};
}

template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };

bool ElFAuxvec::is_64bit() const
{
    return ptr_width_ == PtrWidth::W64bit;
}

size_t ElFAuxvec::auxval_size() const
{
    auto ptr_size = is_64bit() ? sizeof(u64) : sizeof(u32);
    return (auxvec_.size() * 2 + 1) * ptr_size;
}

std::optional<ElFAuxvec::data_out_type> ElFAuxvec::serialize(std::uintptr_t stack_end)
{
    // Serialize args and environment
    klib::vector<std::byte> args_serialized;

    klib::vector<size_t> args_offsets;
    for (const auto &a: args_) {
        auto offset = args_serialized.size();
        if (!args_offsets.push_back(offset))
            return {};

        if (!append_string_bytes(args_serialized, a))
            return {};
    }

    klib::vector<size_t> environment_offset;
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

    klib::vector<size_t> extra_offset;
    for (const auto &a: aux_) {
        auto offset = args_serialized.size();
        if (!extra_offset.push_back(offset))
            return {};

        if (!args_serialized.append_range(a))
            return {};

        size_t new_size = (args_serialized.size() + alignment_mask) & ~alignment_mask;

        if (!args_serialized.resize(new_size, std::byte{0}))
            return {};
    }

    size_t final_size = size_serialized();

    klib::vector<std::byte> output;
    if (!output.reserve(final_size))
        return {};

    uintptr_t start_of_args = stack_end - args_serialized.size();

    // Push arguments, args count and environment
    if (is_64bit()) {
        (void)output.append_range(
            std::bit_cast<std::array<std::byte, sizeof(u64)>>(static_cast<u64>(args_.size())));

        for (auto i: args_offsets) {
            (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(u64)>>(
                static_cast<u64>(i + start_of_args)));
        }

        (void)output.append_range(
            std::bit_cast<std::array<std::byte, sizeof(u64)>>(static_cast<u64>(0)));

        for (auto i: environment_offset) {
            (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(u64)>>(
                static_cast<u64>(i + start_of_args)));
        }

        (void)output.append_range(
            std::bit_cast<std::array<std::byte, sizeof(u64)>>(static_cast<u64>(0)));
    } else {
        (void)output.append_range(
            std::bit_cast<std::array<std::byte, sizeof(u32)>>(static_cast<u32>(args_.size())));

        for (auto i: args_offsets) {
            (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(u32)>>(
                static_cast<u32>(i + start_of_args)));
        }

        (void)output.append_range(
            std::bit_cast<std::array<std::byte, sizeof(u32)>>(static_cast<u32>(0)));

        for (auto i: environment_offset) {
            (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(u32)>>(
                static_cast<u32>(i + start_of_args)));
        }

        (void)output.append_range(
            std::bit_cast<std::array<std::byte, sizeof(u32)>>(static_cast<u32>(0)));
    }

    // Fun part
    if (is_64bit()) {
        struct Serialized {
            u64 type;
            u64 value;
        };

        for (auto i : auxvec_) {
            u64 value = 0;
            if (auto l = std::get_if<long>(&i.value); l) {
                value = static_cast<u64>(*l);
            } else if (auto ptr = std::get_if<uintptr_t>(&i.value); ptr) {
                value = static_cast<u64>(*ptr);
            } else if (auto ii = std::get_if<int>(&i.value); ii) {
                auto t = extra_offset.get(*ii);
                if (!t)
                    return {};

                value = static_cast<u64>(*t + start_of_args);
            } else {
                assert(false);
            }

            Serialized s{static_cast<u64>(i.a_type), value};

            (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(s)>>(s));
        }

        // Null vector...
        (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(u64)>>((u64)0));
    } else {
        struct Serialized {
            u32 type;
            u32 value;
        };

        for (auto i : auxvec_) {
            u32 value = 0;
            if (auto l = std::get_if<long>(&i.value); l) {
                value = static_cast<u32>(*l);
            } else if (auto ptr = std::get_if<uintptr_t>(&i.value); ptr) {
                value = static_cast<u32>(*ptr);
            } else if (auto ii = std::get_if<int>(&i.value); ii) {
                auto t = extra_offset.get(*ii);
                if (!t)
                    return {};

                value = static_cast<u32>(*t + start_of_args);
            } else {
                assert(false);
            }

            Serialized s{static_cast<u32>(i.a_type), value};

            (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(s)>>(s));
        }

        // Null vector...
        (void)output.append_range(std::bit_cast<std::array<std::byte, sizeof(u32)>>((u32)0));
    }

    size_t to_fill = final_size - output.size() - args_serialized.size();
    assert(to_fill <= 12);
    
    (void)output.insert(output.end(), to_fill, std::byte{0});

    (void)output.append_range(std::move(args_serialized));

    return output;
}

ElFAuxvec::PtrWidth ElFAuxvec::set_width(ElFAuxvec::PtrWidth width)
{
    std::swap(ptr_width_, width);
    return width;
}

} // namespace kernel::proc