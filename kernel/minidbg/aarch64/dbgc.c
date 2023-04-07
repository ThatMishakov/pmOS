/*
 * aarch64/dbgc.c
 *
 * Copyright (C) 2021 bzt (bztsrc@github)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * @brief Architecture dependent parts of the mini debugger
 */

void dbg_printf(char *fmt, ...);
extern unsigned long dbg_regs[37];
extern char dbg_cmd[256], dbg_running;

/**
 * Decode exception cause
 */
void dbg_decodeexc(unsigned long type)
{
    unsigned char cause=dbg_regs[33]>>26;

    /* print out interruption type */
    switch(type) {
        case 0: dbg_printf("Synchronous"); break;
        case 1: dbg_printf("IRQ"); break;
        case 2: dbg_printf("FIQ"); break;
        case 3: dbg_printf("SError"); break;
    }
    dbg_printf(": ");
    /* decode exception type (some, not all. See ARM DDI0487B_b chapter D10.2.28) */
    switch(cause) {
        case 0b000000: dbg_printf("Unknown"); break;
        case 0b000001: dbg_printf("Trapped WFI/WFE"); break;
        case 0b001110: dbg_printf("Illegal execution"); break;
        case 0b010101: dbg_printf("System call"); break;
        case 0b100000: dbg_printf("Instruction abort, lower EL"); break;
        case 0b100001: dbg_printf("Instruction abort, same EL"); break;
        case 0b100010: dbg_printf("Instruction alignment fault"); break;
        case 0b100100: dbg_printf("Data abort, lower EL"); break;
        case 0b100101: dbg_printf("Data abort, same EL"); break;
        case 0b100110: dbg_printf("Stack alignment fault"); break;
        case 0b101100: dbg_printf("Floating point"); break;
        case 0b110000: dbg_printf("Breakpoint, lower EL"); break;
        case 0b110001: dbg_printf("Breakpoint, same EL"); break;
        case 0b111100: dbg_printf("Breakpoint instruction"); break;
        default: dbg_printf("Unknown %x", cause); break;
    }
    /* decode data abort cause */
    if(cause==0b100100 || cause==0b100101) {
        dbg_printf(", ");
        switch((dbg_regs[33]>>2)&0x3) {
            case 0: dbg_printf("Address size fault"); break;
            case 1: dbg_printf("Translation fault"); break;
            case 2: dbg_printf("Access flag fault"); break;
            case 3: dbg_printf("Permission fault"); break;
        }
        switch(dbg_regs[33]&0x3) {
            case 0: dbg_printf(" at level 0"); break;
            case 1: dbg_printf(" at level 1"); break;
            case 2: dbg_printf(" at level 2"); break;
            case 3: dbg_printf(" at level 3"); break;
        }
    }
    dbg_printf("\n");
    /* if the exception happened in the debugger, we stop to avoid infinite loop */
    if(dbg_running) {
        dbg_printf("Exception in mini debugger!\n"
            "  elr_el1: %x  spsr_el1: %x\n  esr_el1: %x  far_el1: %x\nsctlr_el1: %x  tcr_el1: %x\n",
            dbg_regs[31],dbg_regs[32],dbg_regs[33],dbg_regs[34],dbg_regs[35],dbg_regs[36]);
        while(1);
    }
}

/**
 * Parse register name
 */
int dbg_getreg(int i, int *reg)
{
    if(dbg_cmd[i]=='x' || dbg_cmd[i]=='r') {
        i++; if(dbg_cmd[i]>='0' && dbg_cmd[i]<='9') { *reg=dbg_cmd[i]-'0'; }
        i++; if(dbg_cmd[i]>='0' && dbg_cmd[i]<='9') { *reg*=10; *reg+=dbg_cmd[i]-'0'; }
    }
    return i;
}

/**
 * Dump registers
 */
void dbg_dumpreg()
{
    int i;
    /* general purpose registers x0-x30 */
    for(i=0;i<31;i++) {
        if(i && i%3==0) dbg_printf("\n");
        if(i<10) dbg_printf(" ");
        dbg_printf("x%d: %16x  ",i,dbg_regs[i]);
    }
    /* some system registers */
    dbg_printf("elr_el1: %x  spsr_el1: %x\n  esr_el1: %x  far_el1: %x\nsctlr_el1: %x  tcr_el1: %x\n",
        dbg_regs[31],dbg_regs[32],dbg_regs[33],dbg_regs[34],dbg_regs[35],dbg_regs[36]);
}
