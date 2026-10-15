#pragma once

namespace kernel::m68k
{

enum class CpuKind {
    M68020,
    M68030,
    M68040,
    M68060,
};

extern CpuKind cpu_kind;

} // namespace kernel::m68k