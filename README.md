# pmOS


A hobbyist operating system for x86_64 written in C, C++ and ASM. The goal of the project is to learn about the OS development and ideally to develop a complex system with GUI and different conveniences you would expect out of one.

## Compilation and execution

GCC/binutils are used as a cross-compiler and a linker, and grub2 is used to create bootable CDs (and loading the kernel). In order to compile the kernel and loader, you need make, grub, xorriso, grub2 and a gcc cross-compiler (https://wiki.osdev.org/GCC_Cross-Compiler). Userspace programs use hosted gcc cross-compiler, which should be built automatically the first time you execute a make. You can then use `make bochs` to compile everything and execute it inside the bochs debugger.

## Features
These are the features that are planned to be had in the OS:

**Kernel and loader:**
- [x] Loader
  - [x] Loading of kernel
  - [x] Loading ELF
  - [x] Starting processes
  - [x] Exiting

- [ ] Processes and threads
  - [ ] Processes - The kernel has tasks, but POSIX processes and execlp/spawn is not yet implemented
  - [x] Process switching
  - [x] Preemptive multitasking
  - [X] Threads - pthreads are very basic
  - [X] Kernel threads
  - [x] Ring 3

- [ ] Memory
  - [x] Page frame allocator
  - [x] Virtual memory manager
  - [x] Allocating memory to userspace
  - [x] Memory mapping
  - [x] Releasing used pages
  - [x] Shared memory
  - [x] Delayed allocation
  - [x] Memory protections (NX bit)
  - [ ] Swapping

- [ ] Interrupts and exceptions
  - [x] Very basic exception handling
  - [x] Syscalls
    - [x] Syscalls with interrupts
    - [x] Fast syscalls with SYSCALL/SYSRET
    - [x] SYSENTER/SYSEXIT (for no reason...)
    - [ ] Call Gates ?
  - [ ] Interrupt dispatching to drivers -> I have written one, but I don't like it and plan on rewriting it

- [x] IPC and messaging
  - [x] Buffered string messages
  - [x] Ports
  - [x] Kernel messages
  - [ ] Quicker messaging

  
- [ ] Permissions
- [x] Multi CPU support -> race conditions all over the place
- [X] SSE instructions

**Core utilities and daemons**
- [ ] Process management (processd)
- [ ] Networking (networkd)
- [ ] Filesystems
  - [ ] FAT32
  - [ ] FUSE
  - [ ] USTAR filesystem daemon - Archive parsing works, but it is very incomplete
  - [ ] VFSd
    - [X] Mounting filesystems
    - [X] Opening files
    - [X] Traversing trees
- [ ] Drawing on screen (screend)
- [ ] Human input
- [X] Native executables
- [ ] POSIX compatibility layer - Many functions are implemented and working, but a lot more are yet to be done

**Drivers**
- [X] PS/2
  - [X] Keyboard
  - [ ] Mouse
- [ ] ATA/Bulk storage
- [ ] USB
- [ ] Serial/Parallel
- [ ] PCI/PCIe
- [ ] Network cards
- [ ] Timers

**Userland**
- [ ] Terminal
- [ ] GUI
- [ ] C Library -> started workking on one, but it's very far from complete

**Languages/Runtimes**
- [X] Hosted GCC cross-compiler
- [X] C and its runtime (using homemade C/POSIX library)
- [X] C++ and libstdc++ (and other) runtime(s) - Compiling and working but a lot of C library functions are missing
- [X] Go and libgo runtime - Compiling, but untested and probably broken for now. Also, exec-stuff is missing


## Architecture

The OS is based on a microkernel architecture, with the idea of running drivers and services in the userspace. The kernel currently has physical and virtual memory managers, scheduling, interrupts, IPC and permissions managed inside it. Of the process management, the kernel only knows about tasks, with (POSIX) processes and threads being an
abstraction on top of it, being managed in a userspace

The executables in ELF format are used

The system supports booting by the multiboot2 protocol. The loader is booted by the bootloader, which then prepares virtual memory as if it was an userspace program, loads the kernel (in elf format), briefly transfers execution to its init function, starts the processes via syscalls and then mounts itself as a ramdisk filesystem, helping processes
continue initialization, having conveniences of the mostly-working runtime

## Known issues

- Compiling C Library with -O2 or O3 breaks booting the system

## License

_TODO_