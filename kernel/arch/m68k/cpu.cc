#include "cpu.hh"
#include <array>
#include <types.hh>

namespace kernel::m68k
{

CpuKind cpu_kind = CpuKind::M68030;

} // namespace kernel::m68k

extern void interrupt_entry();
constinit static const std::array<void (*)(), 256> ivt = []() constexpr {
    std::array<void (*)(), 256> table{};
    for (std::size_t i = 0; i < table.size(); ++i) {
        table[i] = interrupt_entry;
    }
    return table;
}();

// Comment of shame for GCC 16
// extern "C" constinit const std::array<u32, 16> frame_sizes_words = []() constexpr {
//     std::array<u32, 16> sizes{};
//     sizes[0]  = 4;  // Four word
//     sizes[1]  = 4;  // Throwaway four word
//     sizes[2]  = 6; // Six word
//     sizes[3]  = 6; // Six word
//     sizes[4]  = 8; // Eight word
//     sizes[5]  = 0;  // reserved
//     sizes[6]  = 0;  // reserved
//     sizes[7]  = 30; // Access Fault Frame
//     sizes[8]  = 29; // 68010-only 29-word bus fault frame
//     sizes[9]  = 10; // Coprocessor Mid-Instruction
//     sizes[10] = 16; // Short Bus Cycle Fault
//     sizes[11] = 46; // Long Bus Cycle Fault
//     sizes[12] = 0;  // reserved
//     sizes[13] = 0;  // reserved
//     sizes[14] = 0;  // reserved
//     sizes[15] = 0;  // reserved
//     return sizes;
// }();

extern "C" constinit const std::array<u32, 16> frame_sizes_words = {
    4,  // Four word
    4,  // Throwaway four word
    6, // Six word
    6, // Six word
    8, // Eight word
    0,  // reserved
    0,  // reserved
    30, // Access Fault Frame
    29, // 68010-only 29-word bus fault frame
    10, // Coprocessor Mid-Instruction
    16, // Short Bus Cycle Fault
    46, // Long Bus Cycle Fault
    0,  // reserved
    0,  // reserved
    0,  // reserved
    0,  // reserved  
};