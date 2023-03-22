#pragma once
#include "interrupts.hh"
#include "idt.hh"

// This file contains different exception managers.
// See https://wiki.osdev.org/Exceptions for what each one does.

extern "C" void jumpto_func(void) NORETURN;

void kernel_jump_to(void (*function)(void));

constexpr u8 division_error_num = 0x0;
extern "C" void division_error_isr();
extern "C" void division_error_manager();

constexpr u8 debug_trap_num = 0x1;
extern "C" void debug_trap_isr();
extern "C" void debug_trap_manager();

constexpr u8 nmi_num = 0x2;
extern "C" void nmi_isr();
extern "C" void nmi_manager();

constexpr u8 invalid_opcode_num = 0x6;
extern "C" void invalid_opcode_isr();
extern "C" void invalid_opcode_manager();

constexpr u8 sse_exception_num = 0x7;
extern "C" void sse_exception_isr();
extern "C" void sse_exception_manager();

constexpr u8 double_fault_num = 0x8;
extern "C" void double_fault_isr();
extern "C" void double_fault_manager();

constexpr u8 stack_segment_fault_num = 0xC;
extern "C" void stack_segment_fault_isr();
extern "C" void stack_segment_fault_manager();

constexpr u8 general_protection_fault_num = 0xD;
extern "C" void general_protection_fault_isr();
extern "C" void general_protection_fault_manager();

constexpr u8 pagefault_num = 0xE;
extern "C" void pagefault_isr();
extern "C" void pagefault_manager();