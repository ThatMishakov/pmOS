Mini Debugger
=============

This is an extremely simple, small, dependency-free, easily embeddable multiplatform debugger written in ANSI C (and minimal
Assembly). It is supposed to be integrated into kernels and bare metal programs, and it provides a VT terminal interface over
serial line (with 115200 baud, 8 data bits, 1 stop bit, no parity). It's no more than 600 SLoC, and includes a fully featured
[disassembler](https://gitlab.com/bztsrc/udisasm), that's why the compiled code is so "big".

| Architecture | Code size |
|--------------|----------:|
| AArch64      |      124k |
| x86_64       |       74k |

Compilation
-----------

Just run
```sh
$ make
```
This will create `libdbg.a`. You can specify the architecture like this:
```sh
$ ARCH=aarch64 make
$ ARCH=x86_64 make
```
Then link with your kernel like
```sh
$ ld mykernel.o -o mykernel -L./dbg -ldbg
```
If by any chance you encounter linking issues with the static .a file, then you can use
```sh
$ make objs
```
And link with
```sh
$ ld mykernel.o ./dbg/*.o -o mykernel
```

Application Interface
---------------------

### Basic

Pretty simple, include `$(ARCH)/dbg.h`, and initialize interrupt handlers (if you haven't already). After that you'll get the
interactive debugger prompt if anything goes wrong in run-time. You can also place explicit debugger calls with `breakpoint`.
```c
#include <x86_64/dbg.h>

void _start()
{
    /* set up dummy Interrupt Service Routines if you haven't done already */
    dbg_init();

    /* from now on anywhere an exception happens in your code, the mini debugger
       is automatically called. To explicitly invoke it, simply do */
    breakpoint;
}
```
### Integration with your ISRs

If you already have set up interrupt handlers, this is how you can invoke the *mini debugger*. You need to call three functions
in order: `dbg_saveregs`, `dbg_main` and `dbg_loadregs`. These are separated because the first and the third are written in
Assembly, while the second is in C.

```c
void dbg_saveregs()
```
Stores the current register state.

On **x86_64** it expects the stack to be consistent and to contain an interrupt frame (SS, RSP, RFLAGS, CS, RIP, code). This
means for exceptions that do not place an exception code on the stack, you must push 0 before you call it.

On **AArch64** the environment is read from system registers, so the stack is insignificant. However be aware that calling
a function changes a register, so you must push x30 before you make a call. See
[aarch64/dbga.S](https://gitlab.com/bztsrc/minidbg/-/blob/master/aarch64/dbga.S#L137).

```c
void dbg_main(unsigned long excnum)
```
Invokes the debugger which provides the interactive prompt. The `excnum` argument is simply the number of the ISR, used to
identity the exception's cause.

```c
void dbg_loadregs()
```
Restores the current register state. Note that there are similar considerations on AArch64 as with dbg_saveregs().

Usage
-----

### Qemu

Simply add a serial command line parameter, and you'll be able to control the debugger from the same terminal you are using
to run qemu.
```sh
$ qemu (your specific arguments) -serial stdio
```

### Real Machine

On IBM PC, the test machine must have a 16550 or compatible chip. Many new machines emulate that, so it's likely you have it.
Then connect the test machine with an RS-232 cable or [RS-232 USB converter](https://www.adafruit.com/product/954) to another
PC running PuTTY (on Windows), or
```sh
$ minicom -b 115200 -D /dev/ttyUSB0
```
(on Linux). Use the appropriate device, for real serial ports (drived by 16550 chip) use `/dev/ttyS0`.

On Raspberry Pi, use an RS-232 cable or RS-232 USB converter and connect it to GPIO pins 14/15 on the Pi's side. On the other
PC use the same utility as with x86_64.

### Mini Debugger Prompt

When something blows off, you should see a similar output in PuTTY or minicom terminal:
```
Mini debugger by bzt
Synchronous: Breakpoint instruction
> _
```
You can get help with the `?` or `h` command:
```
Mini debugger by bzt
Synchronous: Breakpoint instruction
> ?
Mini debugger commands:
  ?/h               this help
  c                 continue execution
  n                 move to the next instruction
  r                 dump registers
  x [os [oe]]       examine memory from offset start (os) to offset end (oe)
  i [os [oe]]       disassemble instructions from offset start to offset end
> _
```
Offsets can be in decimal and in hex, absolute addresses, or register relative, like
```
> x rbp-8 rbp+16
```
or
```
> i x30+0x200
```

Where you go from here depends on you. *Mini debugger* was never intended to be a fully featured debugger, instead simplicy,
small code size and easy embeddability are its main goals, and to serve you as a skeleton for your own debugger. You can
modify it, expand it, and add more commands if you'd like. Hints: add formatted print for the stack, single step instructions,
ablilty to modify register values etc.

License
-------

Licensed under the terms of the MIT license.

That's all folks!

Cheers,
bzt
