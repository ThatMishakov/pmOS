/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

constexpr int TIMER_INTERRUPT = 5;