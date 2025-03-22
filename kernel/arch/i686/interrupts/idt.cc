#include "idt.hh"

#include "gdt.hh"

#include <interrupts/programmable_ints_functs.hh>

constexpr u64 task_gate(u16 selector) { return 0x0000'8500'0000'0000 | (u64(selector) << 16); }

constexpr u64 interrupt_gate(u32 function, u16 cpl)
{
    return 0x0000'8e00'0000'0000 | ((u64)function & 0xffff) | (u64(cpl) << 45) |
           (u64(function & 0xffff0000) << 32) | (R0_CODE_SEGMENT << 16);
}

constexpr unsigned DIVIDE_INT                      = 0;
constexpr unsigned DEBUG_INT                       = 1;
constexpr unsigned NMI_INT                         = 2;
constexpr unsigned BREAKPOINT_INT                  = 3;
constexpr unsigned OVERFLOW_INT                    = 4;
constexpr unsigned BOUNDS_INT                      = 5;
constexpr unsigned INVALID_OPCODE_INT              = 6;
constexpr unsigned DEVICE_NOT_AVAILABLE_INT        = 7;
constexpr unsigned DOUBLE_FAULT_INT                = 8;
constexpr unsigned COPROCESSOR_SEGMENT_OVERRUN_INT = 9;
constexpr unsigned INVALID_TSS_INT                 = 10;
constexpr unsigned SEGMENT_NOT_PRESENT_INT         = 11;
constexpr unsigned STACK_SEGMENT_FAULT_INT         = 12;
constexpr unsigned GENERAL_PROTECTION_INT          = 13;
constexpr unsigned PAGE_FAULT_INT                  = 14;
constexpr unsigned RESERVED_INT                    = 15;
constexpr unsigned X87_FPU_FP_ERROR_INT            = 16;
constexpr unsigned ALIGNMENT_CHECK_INT             = 17;
constexpr unsigned MACHINE_CHECK_INT               = 18;
constexpr unsigned SIMD_FP_EXCEPTION_INT           = 19;
constexpr unsigned VIRTUALIZATION_INT              = 20;

constexpr unsigned SYSCALL_INT            = 0xf8;
constexpr unsigned APIC_DUMMY_ISR         = 0xf9;
constexpr unsigned IPI_RESCHEDULE_ISR     = 0xfa;
constexpr unsigned IPI_INVALIDATE_TLB_ISR = 0xfb;
constexpr unsigned APIC_TIMER_INT         = 0xfc;
constexpr unsigned APIC_LVT0_ISR          = 0xfd;
constexpr unsigned APIC_LVT1_ISR          = 0xfe;
constexpr unsigned APIC_SPURIOUS_INT      = 0xff;

extern "C" void general_protection_fault_isr();
extern "C" void page_fault_isr();
extern "C" void sse_exception_isr();

extern "C" void syscall_isr();
extern "C" void apic_dummy_isr();
extern "C" void ipi_reschedule_isr();
extern "C" void ipi_invalidate_tlb_isr();
extern "C" void apic_timer_isr();
extern "C" void apic_lvt0_isr();
extern "C" void apic_lvt1_isr();
extern "C" void apic_spurious_isr();

static IDT init_idt()
{
    IDT idt = {};
    auto &u = idt.entries;

    // u[DIVIDE_INT]
    // u[DEBUG_INT] = task_gate(DEBUG_TSS_SEGMENT);
    // u[NMI_INT] = task_gate(NMI_TSS_SEGMENT);
    // u[BREAKPOINT_INT]
    // u[OVERFLOW_INT]
    // u[BOUNDS_INT]
    // u[INVALID_OPCODE_INT]
    u[DEVICE_NOT_AVAILABLE_INT] = interrupt_gate((u32)sse_exception_isr, 0);
    // u[DOUBLE_FAULT_INT] = task_gate(DOUBLE_FAULT_TSS_SEGMENT);
    // u[COPROCESSOR_SEGMENT_OVERRUN_INT]
    // u[INVALID_TSS_INT]
    // u[SEGMENT_NOT_PRESENT_INT]
    // u[STACK_SEGMENT_FAULT_INT] = task_gate(STACK_FAULT_TSS_SEGMENT);
    u[GENERAL_PROTECTION_INT]   = interrupt_gate((u32)general_protection_fault_isr, 0);
    u[PAGE_FAULT_INT]           = interrupt_gate((u32)page_fault_isr, 0);
    // u[RESERVED_INT]
    // u[X87_FPU_FP_ERROR_INT]
    // u[ALIGNMENT_CHECK_INT]
    // u[MACHINE_CHECK_INT] = task_gate(MACHINE_CHECK_TSS_SEGMENT);
    // u[SIMD_FP_EXCEPTION_INT]
    // u[VIRTUALIZATION_INT]

    u[32] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[33] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[34] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[35] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[36] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[37] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[38] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[39] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[40] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[41] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[42] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[43] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[44] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[45] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[46] = interrupt_gate((u32)apic_spurious_isr, 0);
    u[47] = interrupt_gate((u32)apic_spurious_isr, 0);

    u[48] = interrupt_gate((u32)prog_int_48, 0);
    u[49] = interrupt_gate((u32)prog_int_49, 0);
    u[50] = interrupt_gate((u32)prog_int_50, 0);
    u[51] = interrupt_gate((u32)prog_int_51, 0);
    u[52] = interrupt_gate((u32)prog_int_52, 0);
    u[53] = interrupt_gate((u32)prog_int_53, 0);
    u[54] = interrupt_gate((u32)prog_int_54, 0);
    u[55] = interrupt_gate((u32)prog_int_55, 0);
    u[56] = interrupt_gate((u32)prog_int_56, 0);
    u[57] = interrupt_gate((u32)prog_int_57, 0);
    u[58] = interrupt_gate((u32)prog_int_58, 0);
    u[59] = interrupt_gate((u32)prog_int_59, 0);
    u[60] = interrupt_gate((u32)prog_int_60, 0);
    u[61] = interrupt_gate((u32)prog_int_61, 0);
    u[62] = interrupt_gate((u32)prog_int_62, 0);
    u[63] = interrupt_gate((u32)prog_int_63, 0);
    u[64] = interrupt_gate((u32)prog_int_64, 0);
    u[65] = interrupt_gate((u32)prog_int_65, 0);
    u[66] = interrupt_gate((u32)prog_int_66, 0);
    u[67] = interrupt_gate((u32)prog_int_67, 0);
    u[68] = interrupt_gate((u32)prog_int_68, 0);
    u[69] = interrupt_gate((u32)prog_int_69, 0);
    u[70] = interrupt_gate((u32)prog_int_70, 0);
    u[71] = interrupt_gate((u32)prog_int_71, 0);
    u[72] = interrupt_gate((u32)prog_int_72, 0);
    u[73] = interrupt_gate((u32)prog_int_73, 0);
    u[74] = interrupt_gate((u32)prog_int_74, 0);
    u[75] = interrupt_gate((u32)prog_int_75, 0);
    u[76] = interrupt_gate((u32)prog_int_76, 0);
    u[77] = interrupt_gate((u32)prog_int_77, 0);
    u[78] = interrupt_gate((u32)prog_int_78, 0);
    u[79] = interrupt_gate((u32)prog_int_79, 0);
    u[80] = interrupt_gate((u32)prog_int_80, 0);
    u[81] = interrupt_gate((u32)prog_int_81, 0);
    u[82] = interrupt_gate((u32)prog_int_82, 0);
    u[83] = interrupt_gate((u32)prog_int_83, 0);
    u[84] = interrupt_gate((u32)prog_int_84, 0);
    u[85] = interrupt_gate((u32)prog_int_85, 0);
    u[86] = interrupt_gate((u32)prog_int_86, 0);
    u[87] = interrupt_gate((u32)prog_int_87, 0);
    u[88] = interrupt_gate((u32)prog_int_88, 0);
    u[89] = interrupt_gate((u32)prog_int_89, 0);
    u[90] = interrupt_gate((u32)prog_int_90, 0);
    u[91] = interrupt_gate((u32)prog_int_91, 0);
    u[92] = interrupt_gate((u32)prog_int_92, 0);
    u[93] = interrupt_gate((u32)prog_int_93, 0);
    u[94] = interrupt_gate((u32)prog_int_94, 0);
    u[95] = interrupt_gate((u32)prog_int_95, 0);
    u[96] = interrupt_gate((u32)prog_int_96, 0);
    u[97] = interrupt_gate((u32)prog_int_97, 0);
    u[98] = interrupt_gate((u32)prog_int_98, 0);
    u[99] = interrupt_gate((u32)prog_int_99, 0);
    u[100] = interrupt_gate((u32)prog_int_100, 0);
    u[101] = interrupt_gate((u32)prog_int_101, 0);
    u[102] = interrupt_gate((u32)prog_int_102, 0);
    u[103] = interrupt_gate((u32)prog_int_103, 0);
    u[104] = interrupt_gate((u32)prog_int_104, 0);
    u[105] = interrupt_gate((u32)prog_int_105, 0);
    u[106] = interrupt_gate((u32)prog_int_106, 0);
    u[107] = interrupt_gate((u32)prog_int_107, 0);
    u[108] = interrupt_gate((u32)prog_int_108, 0);
    u[109] = interrupt_gate((u32)prog_int_109, 0);
    u[110] = interrupt_gate((u32)prog_int_110, 0);
    u[111] = interrupt_gate((u32)prog_int_111, 0);
    u[112] = interrupt_gate((u32)prog_int_112, 0);
    u[113] = interrupt_gate((u32)prog_int_113, 0);
    u[114] = interrupt_gate((u32)prog_int_114, 0);
    u[115] = interrupt_gate((u32)prog_int_115, 0);
    u[116] = interrupt_gate((u32)prog_int_116, 0);
    u[117] = interrupt_gate((u32)prog_int_117, 0);
    u[118] = interrupt_gate((u32)prog_int_118, 0);
    u[119] = interrupt_gate((u32)prog_int_119, 0);
    u[120] = interrupt_gate((u32)prog_int_120, 0);
    u[121] = interrupt_gate((u32)prog_int_121, 0);
    u[122] = interrupt_gate((u32)prog_int_122, 0);
    u[123] = interrupt_gate((u32)prog_int_123, 0);
    u[124] = interrupt_gate((u32)prog_int_124, 0);
    u[125] = interrupt_gate((u32)prog_int_125, 0);
    u[126] = interrupt_gate((u32)prog_int_126, 0);
    u[127] = interrupt_gate((u32)prog_int_127, 0);
    u[128] = interrupt_gate((u32)prog_int_128, 0);
    u[129] = interrupt_gate((u32)prog_int_129, 0);
    u[130] = interrupt_gate((u32)prog_int_130, 0);
    u[131] = interrupt_gate((u32)prog_int_131, 0);
    u[132] = interrupt_gate((u32)prog_int_132, 0);
    u[133] = interrupt_gate((u32)prog_int_133, 0);
    u[134] = interrupt_gate((u32)prog_int_134, 0);
    u[135] = interrupt_gate((u32)prog_int_135, 0);
    u[136] = interrupt_gate((u32)prog_int_136, 0);
    u[137] = interrupt_gate((u32)prog_int_137, 0);
    u[138] = interrupt_gate((u32)prog_int_138, 0);
    u[139] = interrupt_gate((u32)prog_int_139, 0);
    u[140] = interrupt_gate((u32)prog_int_140, 0);
    u[141] = interrupt_gate((u32)prog_int_141, 0);
    u[142] = interrupt_gate((u32)prog_int_142, 0);
    u[143] = interrupt_gate((u32)prog_int_143, 0);
    u[144] = interrupt_gate((u32)prog_int_144, 0);
    u[145] = interrupt_gate((u32)prog_int_145, 0);
    u[146] = interrupt_gate((u32)prog_int_146, 0);
    u[147] = interrupt_gate((u32)prog_int_147, 0);
    u[148] = interrupt_gate((u32)prog_int_148, 0);
    u[149] = interrupt_gate((u32)prog_int_148, 0);
    u[150] = interrupt_gate((u32)prog_int_150, 0);
    u[151] = interrupt_gate((u32)prog_int_151, 0);
    u[152] = interrupt_gate((u32)prog_int_152, 0);
    u[153] = interrupt_gate((u32)prog_int_153, 0);
    u[154] = interrupt_gate((u32)prog_int_154, 0);
    u[155] = interrupt_gate((u32)prog_int_155, 0);
    u[156] = interrupt_gate((u32)prog_int_156, 0);
    u[157] = interrupt_gate((u32)prog_int_157, 0);
    u[158] = interrupt_gate((u32)prog_int_158, 0);
    u[159] = interrupt_gate((u32)prog_int_159, 0);
    u[160] = interrupt_gate((u32)prog_int_160, 0);
    u[161] = interrupt_gate((u32)prog_int_161, 0);
    u[162] = interrupt_gate((u32)prog_int_162, 0);
    u[163] = interrupt_gate((u32)prog_int_163, 0);
    u[164] = interrupt_gate((u32)prog_int_164, 0);
    u[165] = interrupt_gate((u32)prog_int_165, 0);
    u[166] = interrupt_gate((u32)prog_int_166, 0);
    u[167] = interrupt_gate((u32)prog_int_167, 0);
    u[168] = interrupt_gate((u32)prog_int_168, 0);
    u[169] = interrupt_gate((u32)prog_int_169, 0);
    u[170] = interrupt_gate((u32)prog_int_170, 0);
    u[171] = interrupt_gate((u32)prog_int_171, 0);
    u[172] = interrupt_gate((u32)prog_int_172, 0);
    u[173] = interrupt_gate((u32)prog_int_173, 0);
    u[174] = interrupt_gate((u32)prog_int_174, 0);
    u[175] = interrupt_gate((u32)prog_int_175, 0);
    u[176] = interrupt_gate((u32)prog_int_176, 0);
    u[177] = interrupt_gate((u32)prog_int_177, 0);
    u[178] = interrupt_gate((u32)prog_int_178, 0);
    u[179] = interrupt_gate((u32)prog_int_179, 0);
    u[180] = interrupt_gate((u32)prog_int_180, 0);
    u[181] = interrupt_gate((u32)prog_int_181, 0);
    u[182] = interrupt_gate((u32)prog_int_182, 0);
    u[183] = interrupt_gate((u32)prog_int_183, 0);
    u[184] = interrupt_gate((u32)prog_int_184, 0);
    u[185] = interrupt_gate((u32)prog_int_185, 0);
    u[186] = interrupt_gate((u32)prog_int_186, 0);
    u[187] = interrupt_gate((u32)prog_int_187, 0);
    u[188] = interrupt_gate((u32)prog_int_188, 0);
    u[189] = interrupt_gate((u32)prog_int_189, 0);
    u[190] = interrupt_gate((u32)prog_int_190, 0);
    u[191] = interrupt_gate((u32)prog_int_191, 0);
    u[192] = interrupt_gate((u32)prog_int_192, 0);
    u[193] = interrupt_gate((u32)prog_int_193, 0);
    u[194] = interrupt_gate((u32)prog_int_194, 0);
    u[195] = interrupt_gate((u32)prog_int_195, 0);
    u[196] = interrupt_gate((u32)prog_int_196, 0);
    u[197] = interrupt_gate((u32)prog_int_197, 0);
    u[198] = interrupt_gate((u32)prog_int_198, 0);
    u[199] = interrupt_gate((u32)prog_int_199, 0);
    u[200] = interrupt_gate((u32)prog_int_200, 0);
    u[201] = interrupt_gate((u32)prog_int_201, 0);
    u[202] = interrupt_gate((u32)prog_int_202, 0);
    u[203] = interrupt_gate((u32)prog_int_203, 0);
    u[204] = interrupt_gate((u32)prog_int_204, 0);
    u[205] = interrupt_gate((u32)prog_int_205, 0);
    u[206] = interrupt_gate((u32)prog_int_206, 0);
    u[207] = interrupt_gate((u32)prog_int_207, 0);
    u[208] = interrupt_gate((u32)prog_int_208, 0);
    u[209] = interrupt_gate((u32)prog_int_209, 0);
    u[210] = interrupt_gate((u32)prog_int_210, 0);
    u[211] = interrupt_gate((u32)prog_int_211, 0);
    u[212] = interrupt_gate((u32)prog_int_212, 0);
    u[213] = interrupt_gate((u32)prog_int_213, 0);
    u[214] = interrupt_gate((u32)prog_int_214, 0);
    u[215] = interrupt_gate((u32)prog_int_215, 0);
    u[216] = interrupt_gate((u32)prog_int_216, 0);
    u[217] = interrupt_gate((u32)prog_int_217, 0);
    u[218] = interrupt_gate((u32)prog_int_218, 0);
    u[219] = interrupt_gate((u32)prog_int_219, 0);
    u[220] = interrupt_gate((u32)prog_int_220, 0);
    u[221] = interrupt_gate((u32)prog_int_221, 0);
    u[222] = interrupt_gate((u32)prog_int_222, 0);
    u[223] = interrupt_gate((u32)prog_int_223, 0);
    u[224] = interrupt_gate((u32)prog_int_224, 0);
    u[225] = interrupt_gate((u32)prog_int_225, 0);
    u[226] = interrupt_gate((u32)prog_int_226, 0);
    u[227] = interrupt_gate((u32)prog_int_227, 0);
    u[228] = interrupt_gate((u32)prog_int_228, 0);
    u[229] = interrupt_gate((u32)prog_int_229, 0);
    u[230] = interrupt_gate((u32)prog_int_230, 0);
    u[231] = interrupt_gate((u32)prog_int_231, 0);
    u[232] = interrupt_gate((u32)prog_int_232, 0);
    u[233] = interrupt_gate((u32)prog_int_233, 0);
    u[234] = interrupt_gate((u32)prog_int_234, 0);
    u[235] = interrupt_gate((u32)prog_int_235, 0);
    u[236] = interrupt_gate((u32)prog_int_236, 0);
    u[237] = interrupt_gate((u32)prog_int_237, 0);
    u[238] = interrupt_gate((u32)prog_int_238, 0);
    u[239] = interrupt_gate((u32)prog_int_239, 0);
    u[240] = interrupt_gate((u32)prog_int_240, 0);
    u[241] = interrupt_gate((u32)prog_int_241, 0);
    u[242] = interrupt_gate((u32)prog_int_242, 0);
    u[243] = interrupt_gate((u32)prog_int_243, 0);
    u[244] = interrupt_gate((u32)prog_int_244, 0);
    u[245] = interrupt_gate((u32)prog_int_245, 0);
    u[246] = interrupt_gate((u32)prog_int_246, 0);
    u[247] = interrupt_gate((u32)prog_int_247, 0);


    u[SYSCALL_INT]            = interrupt_gate((u32)syscall_isr, 3);
    u[APIC_DUMMY_ISR]         = interrupt_gate((u32)apic_dummy_isr, 0);
    u[IPI_RESCHEDULE_ISR]     = interrupt_gate((u32)ipi_reschedule_isr, 0);
    u[IPI_INVALIDATE_TLB_ISR] = interrupt_gate((u32)ipi_invalidate_tlb_isr, 0);
    u[APIC_TIMER_INT]         = interrupt_gate((u32)apic_timer_isr, 0);
    u[APIC_LVT0_ISR]          = interrupt_gate((u32)apic_lvt0_isr, 0);
    u[APIC_LVT1_ISR]          = interrupt_gate((u32)apic_lvt1_isr, 0);
    u[APIC_SPURIOUS_INT]      = interrupt_gate((u32)apic_spurious_isr, 0);

    return idt;
}

IDT k_idt = init_idt();