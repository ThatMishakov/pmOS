#pragma once

namespace kernel::loongarch::csr
{

constexpr unsigned ECFG   = 0x04;
constexpr unsigned ESTAT  = 0x05;
constexpr unsigned ERA    = 0x06;
constexpr unsigned BADV   = 0x07;
constexpr unsigned BADI   = 0x08;
constexpr unsigned EENTRY = 0x0C;

constexpr unsigned PGDL = 0x19;
constexpr unsigned PGDH = 0x1A;

}; // namespace kernel::loongarch::csr