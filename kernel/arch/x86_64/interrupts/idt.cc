/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "idt.hh"

#include <interrupts/apic.hh>
#include <interrupts/exceptions_managers.hh>
#include <interrupts/interrupts.hh>
#include <interrupts/programmable_ints_functs.hh>

#include <cpus/ipi.hh>
#include <processes/syscalls.hh>

using namespace kernel;
using namespace kernel::x86_64::interrupts;

// Entry point for when userspace calls software interrupt
extern "C" void syscall_int_entry();

IDT x86_64::interrupts::k_idt = {{
    {},
    {},
    {},
    {&breakpoint_isr, interrupt_gate_type, 2, 0},
    {&overflow_isr, interrupt_gate_type, 0, 0},
    {},
    {&invalid_opcode_isr, interrupt_gate_type, 0, 0},
    {&sse_exception_isr, interrupt_gate_type, 0, 0},
    {&double_fault_isr, interrupt_gate_type, 7, 0},
    {},
    {},
    {},
    {&stack_segment_fault_isr, interrupt_gate_type, 0, 0},
    {&general_protection_fault_isr, interrupt_gate_type, 0, 0},
    {&pagefault_isr, interrupt_gate_type, 0, 0},
    {},
    {},
    {},
    {},
    {&simd_fp_exception_isr, interrupt_gate_type, 0, 0},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&dummy_isr, interrupt_gate_type, 0, 0},
    {&prog_int_48, interrupt_gate_type, 0, 0},
    {&prog_int_49, interrupt_gate_type, 0, 0},
    {&prog_int_50, interrupt_gate_type, 0, 0},
    {&prog_int_51, interrupt_gate_type, 0, 0},
    {&prog_int_52, interrupt_gate_type, 0, 0},
    {&prog_int_53, interrupt_gate_type, 0, 0},
    {&prog_int_54, interrupt_gate_type, 0, 0},
    {&prog_int_55, interrupt_gate_type, 0, 0},
    {&prog_int_56, interrupt_gate_type, 0, 0},
    {&prog_int_57, interrupt_gate_type, 0, 0},
    {&prog_int_58, interrupt_gate_type, 0, 0},
    {&prog_int_59, interrupt_gate_type, 0, 0},
    {&prog_int_60, interrupt_gate_type, 0, 0},
    {&prog_int_61, interrupt_gate_type, 0, 0},
    {&prog_int_62, interrupt_gate_type, 0, 0},
    {&prog_int_63, interrupt_gate_type, 0, 0},
    {&prog_int_64, interrupt_gate_type, 0, 0},
    {&prog_int_65, interrupt_gate_type, 0, 0},
    {&prog_int_66, interrupt_gate_type, 0, 0},
    {&prog_int_67, interrupt_gate_type, 0, 0},
    {&prog_int_68, interrupt_gate_type, 0, 0},
    {&prog_int_69, interrupt_gate_type, 0, 0},
    {&prog_int_70, interrupt_gate_type, 0, 0},
    {&prog_int_71, interrupt_gate_type, 0, 0},
    {&prog_int_72, interrupt_gate_type, 0, 0},
    {&prog_int_73, interrupt_gate_type, 0, 0},
    {&prog_int_74, interrupt_gate_type, 0, 0},
    {&prog_int_75, interrupt_gate_type, 0, 0},
    {&prog_int_76, interrupt_gate_type, 0, 0},
    {&prog_int_77, interrupt_gate_type, 0, 0},
    {&prog_int_78, interrupt_gate_type, 0, 0},
    {&prog_int_79, interrupt_gate_type, 0, 0},
    {&prog_int_80, interrupt_gate_type, 0, 0},
    {&prog_int_81, interrupt_gate_type, 0, 0},
    {&prog_int_82, interrupt_gate_type, 0, 0},
    {&prog_int_83, interrupt_gate_type, 0, 0},
    {&prog_int_84, interrupt_gate_type, 0, 0},
    {&prog_int_85, interrupt_gate_type, 0, 0},
    {&prog_int_86, interrupt_gate_type, 0, 0},
    {&prog_int_87, interrupt_gate_type, 0, 0},
    {&prog_int_88, interrupt_gate_type, 0, 0},
    {&prog_int_89, interrupt_gate_type, 0, 0},
    {&prog_int_90, interrupt_gate_type, 0, 0},
    {&prog_int_91, interrupt_gate_type, 0, 0},
    {&prog_int_92, interrupt_gate_type, 0, 0},
    {&prog_int_93, interrupt_gate_type, 0, 0},
    {&prog_int_94, interrupt_gate_type, 0, 0},
    {&prog_int_95, interrupt_gate_type, 0, 0},
    {&prog_int_96, interrupt_gate_type, 0, 0},
    {&prog_int_97, interrupt_gate_type, 0, 0},
    {&prog_int_98, interrupt_gate_type, 0, 0},
    {&prog_int_99, interrupt_gate_type, 0, 0},
    {&prog_int_100, interrupt_gate_type, 0, 0},
    {&prog_int_101, interrupt_gate_type, 0, 0},
    {&prog_int_102, interrupt_gate_type, 0, 0},
    {&prog_int_103, interrupt_gate_type, 0, 0},
    {&prog_int_104, interrupt_gate_type, 0, 0},
    {&prog_int_105, interrupt_gate_type, 0, 0},
    {&prog_int_106, interrupt_gate_type, 0, 0},
    {&prog_int_107, interrupt_gate_type, 0, 0},
    {&prog_int_108, interrupt_gate_type, 0, 0},
    {&prog_int_109, interrupt_gate_type, 0, 0},
    {&prog_int_110, interrupt_gate_type, 0, 0},
    {&prog_int_111, interrupt_gate_type, 0, 0},
    {&prog_int_112, interrupt_gate_type, 0, 0},
    {&prog_int_113, interrupt_gate_type, 0, 0},
    {&prog_int_114, interrupt_gate_type, 0, 0},
    {&prog_int_115, interrupt_gate_type, 0, 0},
    {&prog_int_116, interrupt_gate_type, 0, 0},
    {&prog_int_117, interrupt_gate_type, 0, 0},
    {&prog_int_118, interrupt_gate_type, 0, 0},
    {&prog_int_119, interrupt_gate_type, 0, 0},
    {&prog_int_120, interrupt_gate_type, 0, 0},
    {&prog_int_121, interrupt_gate_type, 0, 0},
    {&prog_int_122, interrupt_gate_type, 0, 0},
    {&prog_int_123, interrupt_gate_type, 0, 0},
    {&prog_int_124, interrupt_gate_type, 0, 0},
    {&prog_int_125, interrupt_gate_type, 0, 0},
    {&prog_int_126, interrupt_gate_type, 0, 0},
    {&prog_int_127, interrupt_gate_type, 0, 0},
    {&prog_int_128, interrupt_gate_type, 0, 0},
    {&prog_int_129, interrupt_gate_type, 0, 0},
    {&prog_int_130, interrupt_gate_type, 0, 0},
    {&prog_int_131, interrupt_gate_type, 0, 0},
    {&prog_int_132, interrupt_gate_type, 0, 0},
    {&prog_int_133, interrupt_gate_type, 0, 0},
    {&prog_int_134, interrupt_gate_type, 0, 0},
    {&prog_int_135, interrupt_gate_type, 0, 0},
    {&prog_int_136, interrupt_gate_type, 0, 0},
    {&prog_int_137, interrupt_gate_type, 0, 0},
    {&prog_int_138, interrupt_gate_type, 0, 0},
    {&prog_int_139, interrupt_gate_type, 0, 0},
    {&prog_int_140, interrupt_gate_type, 0, 0},
    {&prog_int_141, interrupt_gate_type, 0, 0},
    {&prog_int_142, interrupt_gate_type, 0, 0},
    {&prog_int_143, interrupt_gate_type, 0, 0},
    {&prog_int_144, interrupt_gate_type, 0, 0},
    {&prog_int_145, interrupt_gate_type, 0, 0},
    {&prog_int_146, interrupt_gate_type, 0, 0},
    {&prog_int_147, interrupt_gate_type, 0, 0},
    {&prog_int_148, interrupt_gate_type, 0, 0},
    {&prog_int_149, interrupt_gate_type, 0, 0},
    {&prog_int_150, interrupt_gate_type, 0, 0},
    {&prog_int_151, interrupt_gate_type, 0, 0},
    {&prog_int_152, interrupt_gate_type, 0, 0},
    {&prog_int_153, interrupt_gate_type, 0, 0},
    {&prog_int_154, interrupt_gate_type, 0, 0},
    {&prog_int_155, interrupt_gate_type, 0, 0},
    {&prog_int_156, interrupt_gate_type, 0, 0},
    {&prog_int_157, interrupt_gate_type, 0, 0},
    {&prog_int_158, interrupt_gate_type, 0, 0},
    {&prog_int_159, interrupt_gate_type, 0, 0},
    {&prog_int_160, interrupt_gate_type, 0, 0},
    {&prog_int_161, interrupt_gate_type, 0, 0},
    {&prog_int_162, interrupt_gate_type, 0, 0},
    {&prog_int_163, interrupt_gate_type, 0, 0},
    {&prog_int_164, interrupt_gate_type, 0, 0},
    {&prog_int_165, interrupt_gate_type, 0, 0},
    {&prog_int_166, interrupt_gate_type, 0, 0},
    {&prog_int_167, interrupt_gate_type, 0, 0},
    {&prog_int_168, interrupt_gate_type, 0, 0},
    {&prog_int_169, interrupt_gate_type, 0, 0},
    {&prog_int_170, interrupt_gate_type, 0, 0},
    {&prog_int_171, interrupt_gate_type, 0, 0},
    {&prog_int_172, interrupt_gate_type, 0, 0},
    {&prog_int_173, interrupt_gate_type, 0, 0},
    {&prog_int_174, interrupt_gate_type, 0, 0},
    {&prog_int_175, interrupt_gate_type, 0, 0},
    {&prog_int_176, interrupt_gate_type, 0, 0},
    {&prog_int_177, interrupt_gate_type, 0, 0},
    {&prog_int_178, interrupt_gate_type, 0, 0},
    {&prog_int_179, interrupt_gate_type, 0, 0},
    {&prog_int_180, interrupt_gate_type, 0, 0},
    {&prog_int_181, interrupt_gate_type, 0, 0},
    {&prog_int_182, interrupt_gate_type, 0, 0},
    {&prog_int_183, interrupt_gate_type, 0, 0},
    {&prog_int_184, interrupt_gate_type, 0, 0},
    {&prog_int_185, interrupt_gate_type, 0, 0},
    {&prog_int_186, interrupt_gate_type, 0, 0},
    {&prog_int_187, interrupt_gate_type, 0, 0},
    {&prog_int_188, interrupt_gate_type, 0, 0},
    {&prog_int_189, interrupt_gate_type, 0, 0},
    {&prog_int_190, interrupt_gate_type, 0, 0},
    {&prog_int_191, interrupt_gate_type, 0, 0},
    {&prog_int_192, interrupt_gate_type, 0, 0},
    {&prog_int_193, interrupt_gate_type, 0, 0},
    {&prog_int_194, interrupt_gate_type, 0, 0},
    {&prog_int_195, interrupt_gate_type, 0, 0},
    {&prog_int_196, interrupt_gate_type, 0, 0},
    {&prog_int_197, interrupt_gate_type, 0, 0},
    {&prog_int_198, interrupt_gate_type, 0, 0},
    {&prog_int_199, interrupt_gate_type, 0, 0},
    {&prog_int_200, interrupt_gate_type, 0, 0},
    {&prog_int_201, interrupt_gate_type, 0, 0},
    {&prog_int_202, interrupt_gate_type, 0, 0},
    {&prog_int_203, interrupt_gate_type, 0, 0},
    {&prog_int_204, interrupt_gate_type, 0, 0},
    {&prog_int_205, interrupt_gate_type, 0, 0},
    {&prog_int_206, interrupt_gate_type, 0, 0},
    {&prog_int_207, interrupt_gate_type, 0, 0},
    {&prog_int_208, interrupt_gate_type, 0, 0},
    {&prog_int_209, interrupt_gate_type, 0, 0},
    {&prog_int_210, interrupt_gate_type, 0, 0},
    {&prog_int_211, interrupt_gate_type, 0, 0},
    {&prog_int_212, interrupt_gate_type, 0, 0},
    {&prog_int_213, interrupt_gate_type, 0, 0},
    {&prog_int_214, interrupt_gate_type, 0, 0},
    {&prog_int_215, interrupt_gate_type, 0, 0},
    {&prog_int_216, interrupt_gate_type, 0, 0},
    {&prog_int_217, interrupt_gate_type, 0, 0},
    {&prog_int_218, interrupt_gate_type, 0, 0},
    {&prog_int_219, interrupt_gate_type, 0, 0},
    {&prog_int_220, interrupt_gate_type, 0, 0},
    {&prog_int_221, interrupt_gate_type, 0, 0},
    {&prog_int_222, interrupt_gate_type, 0, 0},
    {&prog_int_223, interrupt_gate_type, 0, 0},
    {&prog_int_224, interrupt_gate_type, 0, 0},
    {&prog_int_225, interrupt_gate_type, 0, 0},
    {&prog_int_226, interrupt_gate_type, 0, 0},
    {&prog_int_227, interrupt_gate_type, 0, 0},
    {&prog_int_228, interrupt_gate_type, 0, 0},
    {&prog_int_229, interrupt_gate_type, 0, 0},
    {&prog_int_230, interrupt_gate_type, 0, 0},
    {&prog_int_231, interrupt_gate_type, 0, 0},
    {&prog_int_232, interrupt_gate_type, 0, 0},
    {&prog_int_233, interrupt_gate_type, 0, 0},
    {&prog_int_234, interrupt_gate_type, 0, 0},
    {&prog_int_235, interrupt_gate_type, 0, 0},
    {&prog_int_236, interrupt_gate_type, 0, 0},
    {&prog_int_237, interrupt_gate_type, 0, 0},
    {&prog_int_238, interrupt_gate_type, 0, 0},
    {&prog_int_239, interrupt_gate_type, 0, 0},
    {&prog_int_240, interrupt_gate_type, 0, 0},
    {&prog_int_241, interrupt_gate_type, 0, 0},
    {&prog_int_242, interrupt_gate_type, 0, 0},
    {&prog_int_243, interrupt_gate_type, 0, 0},
    {&prog_int_244, interrupt_gate_type, 0, 0},
    {&prog_int_245, interrupt_gate_type, 0, 0},
    {&prog_int_246, interrupt_gate_type, 0, 0},
    {&prog_int_247, interrupt_gate_type, 0, 0},
    {&syscall_int_entry, interrupt_gate_type, 0, 3},
    {&apic_dummy_isr, interrupt_gate_type, 0, 0},
    {&ipi_reschedule_isr, interrupt_gate_type, 0, 0},
    {&ipi_invalidate_tlb_isr, interrupt_gate_type, 0, 0},
    {&apic_timer_isr, interrupt_gate_type, 0, 0},
    {&lvt0_int_isr, interrupt_gate_type, 0, 0},
    {&lvt1_int_isr, interrupt_gate_type, 0, 0},
    {&apic_spurious_isr, interrupt_gate_type, 0, 0},
}};