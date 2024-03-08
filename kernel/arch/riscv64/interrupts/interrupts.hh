#pragma once

constexpr int INSTRUCTION_ADDR_MISALIGNED = 0;
constexpr int INSTRUCTION_ACCESS_FAULT = 1;
constexpr int ILLEGAL_INSTRUCTION = 2;
constexpr int BREAKPOINT = 3;
constexpr int LOAD_ADDR_MISALIGNED = 4;
constexpr int LOAD_ACCESS_FAULT = 5;
constexpr int STORE_AMO_ADDR_MISALIGNED = 6;
constexpr int STORE_AMO_ACCESS_FAULT = 7;
constexpr int ENV_CALL_FROM_U_MODE = 8;
constexpr int ENV_CALL_FROM_S_MODE = 9;
constexpr int INSTRUCTION_PAGE_FAULT = 12;
constexpr int LOAD_PAGE_FAULT = 13;
constexpr int STORE_AMO_PAGE_FAULT = 15;
