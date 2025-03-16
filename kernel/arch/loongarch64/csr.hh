#pragma once

namespace kernel::loongarch::csr
{

constexpr unsigned CRMD   = 0x00;
constexpr unsigned PRMD   = 0x01;
constexpr unsigned EUEN   = 0x02;
constexpr unsigned MISC   = 0x03;
constexpr unsigned ECFG   = 0x04;
constexpr unsigned ESTAT  = 0x05;
constexpr unsigned ERA    = 0x06;
constexpr unsigned BADV   = 0x07;
constexpr unsigned BADI   = 0x08;
constexpr unsigned EENTRY = 0x0C;

constexpr unsigned PGDL = 0x19;
constexpr unsigned PGDH = 0x1A;

constexpr unsigned SAVE0 = 0x30;

constexpr unsigned TICLR = 0x44;

}; // namespace kernel::loongarch::csr