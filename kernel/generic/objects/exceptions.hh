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

extern "C" {

typedef enum {
    _URC_NO_REASON                = 0,
    _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
    _URC_FATAL_PHASE2_ERROR       = 2,
    _URC_FATAL_PHASE1_ERROR       = 3,
    _URC_NORMAL_STOP              = 4,
    _URC_END_OF_STACK             = 5,
    _URC_HANDLER_FOUND            = 6,
    _URC_INSTALL_CONTEXT          = 7,
    _URC_CONTINUE_UNWIND          = 8
} _Unwind_Reason_Code;

using uint64 = unsigned long;

struct _Unwind_Exception;

typedef void (*_Unwind_Exception_Cleanup_Fn)(_Unwind_Reason_Code reason,
                                             struct _Unwind_Exception *exc);

struct _Unwind_Exception {
    uint64 exception_class;
    _Unwind_Exception_Cleanup_Fn exception_cleanup;
    uint64 private_1;
    uint64 private_2;
};

struct Registers {
    uint64 registers[16];
    uint64 IP;
};

struct _Unwind_Context {
    Registers regs;
};

_Unwind_Reason_Code _Unwind_RaiseException(_Unwind_Exception *exception_object);

typedef _Unwind_Reason_Code (*_Unwind_Stop_Fn)(int version, _Unwind_Action actions,
                                               uint64 exceptionClass,
                                               struct _Unwind_Exception *exceptionObject,
                                               struct _Unwind_Context *context,
                                               void *stop_parameter);

_Unwind_Reason_Code _Unwind_ForcedUnwind(struct _Unwind_Exception *exception_object,
                                         _Unwind_Stop_Fn stop, void *stop_parameter);

void _Unwind_Resume(struct _Unwind_Exception *exception_object);

void _Unwind_DeleteException(struct _Unwind_Exception *exception_object);

uint64 _Unwind_GetGR(struct _Unwind_Context *context, int index);

void _Unwind_SetGR(struct _Unwind_Context *context, int index, uint64 new_value);

uint64 _Unwind_GetIP(struct _Unwind_Context *context);

void _Unwind_SetIP(struct _Unwind_Context *context, uint64 new_value);

uint64 _Unwind_GetLanguageSpecificData(struct _Unwind_Context *context);

uint64 _Unwind_GetRegionStart(struct _Unwind_Context *context);

_Unwind_Reason_Code (*__personality_routine)(int version, _Unwind_Action actions,
                                             uint64 exceptionClass,
                                             struct _Unwind_Exception *exceptionObject,
                                             struct _Unwind_Context *context);
};