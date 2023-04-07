/*
 * x86_64/dbgc.c
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

void dbg_uart_init();
void dbg_printf(char *fmt, ...);
extern unsigned long dbg_regs[37];
extern char dbg_cmd[256], dbg_running;

char *dbg_regnames[] = { "ax", "bx", "cx", "dx", "si", "di", "sp", "bp" };

/**
 * Decode exception cause
 */
void dbg_decodeexc(unsigned long type)
{
    int i;
    char *exc[] = { "Div zero", "Debug", "NMI", "Breakpoint instruction", "Overflow", "Bound", "Invopcode", "DevUnavail",
        "DblFault", "CoProc", "InvTSS", "SegFault", "StackFault", "GenProt", "PageFault", "Unknown", "Float", "Alignment",
        "MachineCheck", "Double" };
    if(type > 31)
        dbg_printf("Interrupt %02x: IRQ %d\n", type, type - 32);
    else
        dbg_printf("Exception %02x: %s, code %x\n", type, type < 20 ? exc[type] : "Unknown", dbg_regs[23]);
    for(i = 0; i < 8; i++) {
        if(i && i%3==0) dbg_printf("\n");
        dbg_printf("r%s: %16x  ",dbg_regnames[i],dbg_regs[i]);
    }
    for(i = 8; i < 16; i++) {
        if(i && i%3==0) dbg_printf("\n");
        if(i<10) dbg_printf(" ");
        dbg_printf("r%d: %16x  ",i,dbg_regs[i]);
    }
    dbg_printf("\n");
    /* if the exception happened in the debugger, we stop to avoid infinite loop */
    if(dbg_running) {
        dbg_printf("Exception in mini debugger!\n"
            "  rip: %x  cr0: %x\n  cr1: %x  cr2: %x\n  cr3: %x  cr4: %x\nflags: %x code: %x\n",
            dbg_regs[16],dbg_regs[17],dbg_regs[18],dbg_regs[19],dbg_regs[20],dbg_regs[21],dbg_regs[22],dbg_regs[23]);
        while(1);
    }
}

/**
 * Parse register name
 */
int dbg_getreg(int i, int *reg)
{
    int j;
    /* gprs */
    if(dbg_cmd[i]=='r' && ((dbg_cmd[i+1]>='a' && dbg_cmd[i+1]<='d') || dbg_cmd[i+1]=='s')) {
        i++;
        for(j = 0; j < 8; j++)
            if(dbg_cmd[i]==dbg_regnames[j][0] && dbg_cmd[i+1]==dbg_regnames[j][1]) { *reg=j; break; }
    } else
    /* r8 - r15 */
    if(dbg_cmd[i]=='r' && dbg_cmd[i+1]>='0' && dbg_cmd[i+1]<='9') {
        i++; *reg=dbg_cmd[i]-'0';
        i++; if(dbg_cmd[i]>='0' && dbg_cmd[i]<='9') { *reg *= 10; *reg += dbg_cmd[i]-'0'; }
        *reg += 8;
    } else
    /* cr0 - cr4 */
    if(dbg_cmd[i]=='c' && dbg_cmd[i+1]=='r' && dbg_cmd[i+2]>='0' && dbg_cmd[i+2]<='4') {
        i+=2; *reg=dbg_cmd[i]-'0'+16;
    }
    return i;
}

/**
 * Dump registers
 */
void dbg_dumpreg()
{
    int i;
    /* gprs */
    for(i = 0; i < 8; i++) {
        if(i && i%3==0) dbg_printf("\n");
        dbg_printf("r%s: %16x  ",dbg_regnames[i],dbg_regs[i]);
    }
    /* r8 - r15 */
    for(i = 8; i < 16; i++) {
        if(i && i%3==0) dbg_printf("\n");
        if(i<10) dbg_printf(" ");
        dbg_printf("r%d: %16x  ",i,dbg_regs[i]);
    }
    dbg_printf("\n");
    /* some system registers */
    if(dbg_running) {
        dbg_printf("  rip: %x  cr0: %x\n  cr1: %x  cr2: %x\n  cr3: %x  cr4: %x\nflags: %x code: %x\n",
            dbg_regs[16],dbg_regs[17],dbg_regs[18],dbg_regs[19],dbg_regs[20],dbg_regs[21],dbg_regs[22],dbg_regs[23]);
        // while(1);
    }
}
