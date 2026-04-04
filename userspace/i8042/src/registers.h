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

#ifndef REGISTERS_H
#define REGISTERS_H

#define DATA_PORT 0x60
#define RW_PORT   0x64

#define CMD_CONFIG_READ  0x20
#define CMD_CONFIG_WRITE 0x60

#define ENABLE_SECOND_PORT  0xA8
#define DISABLE_SECOND_PORT 0xA7
#define TEST_SECOND_PORT    0xA9

#define TEST_CONTROLLER 0xAA
#define TEST_FIRST_PORT 0xAB

#define DIAGNOSTIC_DUMP 0xAC

#define DISABLE_FIRST_PORT 0xAD
#define ENABLE_FIRST_PORT  0xAE

#define READ_OUTPUT  0xD0
#define WRITE_OUTPUT 0xD1

#define PULSE_RESET 0xF0

#define SECOND_PORT 0xD4

#define STATUS_MASK_OUTPUT_FULL 0x01
#define STATUS_MASK_INPUT_FULL  0x02
#define STATUS_MASK_FLAG_SYSTEM 0x04
#define STATUS_MASK_DATA_CMD    0x08
#define STATUS_MASK_KB_LOCK     0x10
#define STATUS_MASK_SECOND_FULL 0x20
#define STATUS_MASK_TIMEOUT     0x40
#define STATUS_MASK_PARITY_ERR  0x80

#endif