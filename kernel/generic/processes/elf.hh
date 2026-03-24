#pragma once
#include <lib/vector.hh>
#include <lib/string.hh>
#include <types.hh>
#include <utility>
#include <cstddef>

namespace kernel::proc
{

class ElFAuxvec
{
public:
    using data_out_type = klib::vector<std::byte>;

    struct AuxVecVal {
        int a_type;
        std::variant<long, uintptr_t, int> value;
        // 0 - long plain value
        // 1 - pointer or function pointer
        // 2 - index in the aux information array...
    };

    enum class PtrWidth {
        W32bit,
        W64bit,
    };

    klib::vector<klib::string> &args();
    const klib::vector<klib::string> &args() const;

    klib::vector<klib::string> &envp();
    const klib::vector<klib::string> &envp() const;

    klib::vector<klib::vector<std::byte>> &extra_info();
    const klib::vector<klib::vector<std::byte>> &extra_info() const;

    klib::vector<AuxVecVal> &auxvec();
    const klib::vector<AuxVecVal> &auxvec() const;

    // Sets the width for serialization, and returns the previous one
    PtrWidth set_width(PtrWidth width);

    // Size in bytes of the serialized data
    size_t size_serialized() const;

    // Vector containing serialized data (or nothing on error)
    std::optional<data_out_type> serialize(std::uintptr_t stack_end);

    bool is_64bit() const;
protected:
    klib::vector<klib::string> args_;
    klib::vector<klib::string> envp_;
    klib::vector<klib::vector<std::byte>> aux_; // Auxiliary info (each entry aligned to 4 or 8)
    klib::vector<AuxVecVal> auxvec_;
    PtrWidth ptr_width_;

    size_t auxval_size() const;
    size_t strings_size_aligned() const;
};

} // namespace kernel::proc