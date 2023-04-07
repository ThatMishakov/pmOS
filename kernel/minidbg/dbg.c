/*
 * dbg.c
 * https://gitlab.com/bztsrc/minidbg
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
 * @brief: mini debugger architecture independent code
 */

/* add includes here if you want */

#ifndef _STDINT_H
typedef unsigned long   uint64_t;
typedef unsigned int    uint32_t;
typedef unsigned short  uint16_t;
typedef unsigned char   uint8_t;
typedef long            int64_t;
typedef int             int32_t;
typedef short           int16_t;
typedef char            int8_t;
#endif
#ifndef NULL
#define NULL ((void*)0)
#endif
#ifndef true
#define true 1
#define false 0
#endif

/* architecture specific functions */
int dbg_getreg(int i, int *reg);
void dbg_uart_init();
void dbg_decodeexc(unsigned long type);
void dbg_dumpreg();
void dbg_uart_putc(unsigned int c);
char dbg_uart_getc();

/* array to store register values (see dbg_saveregs in arch) */
unsigned long dbg_regs[37];
char dbg_cmd[256], dbg_running=0;

/**
 * minimal sprintf implementation
 */
unsigned int dbg_vsprintf(char *dst, char* fmt, __builtin_va_list args)
{
    long int arg;
    int len, sign, i;
    char *p, *orig=dst, tmpstr[19], pad=' ';

    if(dst==(void*)0 || fmt==(void*)0)
        return 0;

    arg = 0;
    while(*fmt) {
        if(*fmt=='%') {
            fmt++;
            if(*fmt=='%') goto put;
            len=0;
            if(*fmt=='0') pad='0';
            while(*fmt>='0' && *fmt<='9') {
                len *= 10;
                len += *fmt-'0';
                fmt++;
            }
            if(*fmt=='l') fmt++;
            if(*fmt=='c') {
                arg = __builtin_va_arg(args, int);
                *dst++ = (char)arg;
                fmt++;
                continue;
            } else
            if(*fmt=='d') {
                arg = __builtin_va_arg(args, int);
                sign=0;
                if((int)arg<0) {
                    arg*=-1;
                    sign++;
                }
                if(arg>99999999999999999L) {
                    arg=99999999999999999L;
                }
                i=18;
                tmpstr[i]=0;
                do {
                    tmpstr[--i]='0'+(arg%10);
                    arg/=10;
                } while(arg!=0 && i>0);
                if(sign) {
                    tmpstr[--i]='-';
                }
                if(len>0 && len<18) {
                    while(i>18-len) {
                        tmpstr[--i]=pad;
                    }
                }
                p=&tmpstr[i];
                goto copystring;
            } else
            if(*fmt=='x') {
                arg = __builtin_va_arg(args, long int);
                i=16;
                tmpstr[i]=0;
                do {
                    char n=arg & 0xf;
                    /* 0-9 => '0'-'9', 10-15 => 'A'-'F' */
                    tmpstr[--i]=n+(n>9?0x37:0x30);
                    arg>>=4;
                } while(arg!=0 && i>0);
                /* padding, only leading zeros */
                if(len>0 && len<=16) {
                    while(i>16-len) {
                        tmpstr[--i]='0';
                    }
                }
                p=&tmpstr[i];
                goto copystring;
            } else
            if(*fmt=='s') {
                p = __builtin_va_arg(args, char*);
copystring:     if(p==(void*)0) {
                    p="(null)";
                }
                while(*p) {
                    *dst++ = *p++;
                }
            }
        } else {
put:        *dst++ = *fmt;
        }
        fmt++;
    }
    *dst=0;
    return dst-orig;
}

/**
 * Variable length arguments
 */
unsigned int dbg_sprintf(char *dst, char* fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    return dbg_vsprintf(dst,fmt,args);
}

/**
 * Display a string
 */
void dbg_printf(char *fmt, ...)
{
    char tmp[256], *s = (char*)&tmp;
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    dbg_vsprintf(s,fmt,args);
    while(*s) {
        if(*s=='\n')
            dbg_uart_putc('\r');
        dbg_uart_putc(*s++);
    }
}

/**
 * Get the disassembler
 */
#define sprintf dbg_sprintf
#include "disasm.h"
#undef sprintf

/**
 * helper to read a line from user. We redefine some control caracters to handle CSI
 * \e[3~ = 1, delete
 * \e[D  = 2, cursor left
 * \e[C  = 3, cursor right
 */
void dbg_getline()
{
    int i,dbg_cmdidx=0,dbg_cmdlast=0;
    char c;
    dbg_cmd[0]=0;
    dbg_printf("\r> ");
    while((c=dbg_uart_getc())!='\n') {
        /* decode CSI key sequences (some, not all) */
        if(c==27) {
            c=dbg_uart_getc();
            if(c=='[') {
                c=dbg_uart_getc();
                if(c=='C') c=3; else    /* left */
                if(c=='D') c=2; else    /* right */
                if(c=='3') {
                    c=dbg_uart_getc();
                    if(c=='~') c=1;     /* delete */
                }
            }
        }
        /* Backspace */
        if(c==8 || c==127) {
            if(dbg_cmdidx>0) {
                dbg_cmdidx--;
                for(i=dbg_cmdidx;i<dbg_cmdlast;i++) dbg_cmd[i]=dbg_cmd[i+1];
                dbg_cmdlast--;
            }
        } else
        /* Delete */
        if(c==1) {
            if(dbg_cmdidx<dbg_cmdlast) {
                for(i=dbg_cmdidx;i<dbg_cmdlast;i++) dbg_cmd[i]=dbg_cmd[i+1];
                dbg_cmdlast--;
            }
        } else
        /* cursor left */
        if(c==2) {
            if(dbg_cmdidx>0) dbg_cmdidx--;
        } else
        /* cursor right */
        if(c==3) {
            if(dbg_cmdidx<dbg_cmdlast) dbg_cmdidx++;
        } else {
            /* is there a valid character and space to store it? */
            if(c<' ' || dbg_cmdlast >= (int)sizeof(dbg_cmd)-1) {
                continue;
            }
            if(dbg_cmdidx<dbg_cmdlast) {
                for(i=dbg_cmdlast;i>dbg_cmdidx;i--)
                    dbg_cmd[i]=dbg_cmd[i-1];
            }
            dbg_cmdlast++;
            dbg_cmd[dbg_cmdidx++]=c;
        }
        dbg_cmd[dbg_cmdlast]=0;
        /* display prompt and command line, place cursor with CSI code */
        dbg_printf("\r> %s \r\033[%dC",dbg_cmd,dbg_cmdidx+2);
    }
    dbg_printf("\n");
}

/**
 * helper function to parse the command line for arguments
 */
unsigned long dbg_getoffs(int i)
{
    unsigned long base=0,ret=0;
    int j=0,sign=0;
    /* if starts with a register */
    if(dbg_cmd[i]>='a' && dbg_cmd[i]<='z') {
        i = dbg_getreg(i, &j);
        if(j>=0 && j<37) base=dbg_regs[j];
        i++;
        if(dbg_cmd[i]=='-') { i++; sign++; }
        if(dbg_cmd[i]=='+') i++;
    }
    /* offset part */
    if(dbg_cmd[i]=='0' && dbg_cmd[i+1]=='x') {
        i+=2;
        /* hex value */
        while((dbg_cmd[i]>='0'&&dbg_cmd[i]<='9')||(dbg_cmd[i]>='a'&&dbg_cmd[i]<='f')||(dbg_cmd[i]>='A'&&dbg_cmd[i]<='F')) {
            ret <<= 4;
            if(dbg_cmd[i]>='0' && dbg_cmd[i]<='9') ret += dbg_cmd[i]-'0';
            else if(dbg_cmd[i] >= 'a' && dbg_cmd[i] <= 'f') ret += dbg_cmd[i]-'a'+10;
            else if(dbg_cmd[i] >= 'A' && dbg_cmd[i] <= 'F') ret += dbg_cmd[i]-'A'+10;
            i++;
        }
    } else {
        /* decimal value */
        while(dbg_cmd[i]>='0'&&dbg_cmd[i]<='9'){
            ret *= 10;
            ret += dbg_cmd[i++]-'0';
        }
    }
    /* return base + offset */
    return sign? base-ret : base+ret;
}

void serial_print_stack_trace(void * s);

/**
 * main loop, get and parse commands
 */
void dbg_main(unsigned long excnum)
{
    unsigned long os=0, oe=0, a;
    char c;
    char str[64];
    int i, j;

    if(!dbg_running) {
        dbg_uart_init();
        dbg_printf("Mini debugger by bzt\n");
    }
    dbg_decodeexc(excnum);

    dbg_running++;

    /* main debugger loop */
    while(1) {
        /* get command from user */
        dbg_getline();
        /* parse commands */
        if(dbg_cmd[0]==0 || dbg_cmd[0]=='?' || dbg_cmd[0]=='h') {
            dbg_printf("Mini debugger commands:\n"
                "  ?/h\t\tthis help\n"
                "  c\t\tcontinue execution\n"
                "  n\t\tmove to the next instruction\n"
                "  r\t\tdump registers\n"
                "  s\t\tshow stack trace\n"
                "  S\t\tdump stack contents\n"
                "  x [os [oe]]\texamine memory from offset start (os) to offset end (oe)\n"
                "  i [os [oe]]\tdisassemble instructions from offset start to offset end\n");
            continue;
        } else
        /* continue */
        if(dbg_cmd[0]=='c') break;
        /* next instruction */
        if(dbg_cmd[0]=='n') {
            dbg_regs[31]=disasm(dbg_regs[31]?dbg_regs[31]:dbg_regs[30],NULL);
            dbg_cmd[0]='i';
            goto dis;
        } else
        /* dump registers */
        if(dbg_cmd[0]=='r') {
            dbg_dumpreg();
            continue;
        } else
        /* show stack trace */
        if(dbg_cmd[0]=='s') {
            serial_print_stack_trace((void*)dbg_regs[7]);
            continue;
        } else
        /* examine or disassemble, commands with arguments */
        if(dbg_cmd[0]=='x' || dbg_cmd[0]=='i') {
            i=1;
            while(dbg_cmd[i]!=0 && dbg_cmd[i]!=' ') i++;
            while(dbg_cmd[i]!=0 && dbg_cmd[i]==' ') i++;
            if(dbg_cmd[i]!=0) {
                os=oe=dbg_getoffs(i);
                while(dbg_cmd[i]!=0 && dbg_cmd[i]!=' ') i++;
                while(dbg_cmd[i]!=0 && dbg_cmd[i]==' ') i++;
                if(dbg_cmd[i]!=0) {
                    oe=dbg_getoffs(i);
                }
            } else {
                /* no arguments, use defaults */
                if(dbg_cmd[0]=='i') {
dis:                os=oe=dbg_regs[31]?dbg_regs[31]:dbg_regs[30];
                } else {
                    os=oe=dbg_regs[29];
                }
            }
            /* do the thing */
            if(dbg_cmd[0]=='i') {
                /* disassemble bytecode */
                while(os<=oe) {
                    a=os;
                    os=disasm(os,str);
                    dbg_printf("%8x:",a);
                    for(j = 32; a < os; a++, j -= 3) {
                        dbg_printf(" %02x",*((unsigned char*)a));
                    }
                    for(; j > 0; j--)
                        dbg_printf(" ");
                    dbg_printf("%s\n",str);
                }
            } else {
                /* dump memory */
                if(oe<=os) oe=os+16;
                for(a=os;a<oe;a+=16) {
                    dbg_printf("%8x: ", a);
                    for(i=0;i<16;i++) {
                        dbg_printf("%2x%s ",*((unsigned char*)(a+i)),i%4==3?" ":"");
                    }
                    for(i=0;i<16;i++) {
                        c=*((unsigned char*)(a+i));
                        dbg_printf("%c",c<32||c>=127?'.':c);
                    }
                    dbg_printf("\n");
                }
            }
            continue;
        } else {
            dbg_printf("ERROR: unknown command.\n");
        }
    }
    dbg_running--;
}