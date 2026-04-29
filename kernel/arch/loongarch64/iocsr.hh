#pragma once

namespace kernel::loongarch::iocsr {

constexpr u32 IPI_STATUS = 0x1000;
constexpr u32 IPI_ENABLE = 0x1004;
constexpr u32 IPI_SET = 0x1008;
constexpr u32 IPI_CLEAR = 0x100c;
constexpr u32 IPI_SEND = 0x1040;
constexpr u32 IPI_MAIL_SEND = 0x1048;

}