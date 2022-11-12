# pmOS


A hobbyist operating system for x86_64 written in C, C++ and ASM. The goal of the project is to learn about the OS development and ideally to develop a complex system with GUI and different conveniences you would expect out of one.

## Compilation and execution

clang and lld is used as cross-compiler and linker, and grub2 is used to create bootable CDs (and loading the kernel). In order to compile the system, you need make, clang, lld, grub, xorriso, grub2. You can then use `make bochs` to compile everything and execute it inside the bochs debugger.

## Features
These are the features that are planned to be had in the OS:

**Kernel and loader:**
- [x] Loader
  - [x] Loading of kernel
  - [x] Loading ELF
  - [x] Starting processes
  - [x] Exiting

- [ ] Processes and threads
  - [x] Processes
  - [x] Process switching
  - [ ] Preemptive multitasking
  - [ ] Threads
  - [ ] Kernel threads
  - [x] Ring 3

- [ ] Memory
  - [x] Page frame allocator
  - [x] Virtual memory manager
  - [x] Allocating memory to userspace
  - [x] Memory mapping
  - [x] Releasing used pages
  - [x] Shared memory
  - [x] Delayed allocation
  - [ ] Memory protections (NX bit)

- [ ] Interrupts and exceptions
  - [x] Very basic exception handling
  - [x] Syscalls
    - [x] Syscalls with interrupts
    - [ ] Fast syscalls
  - [ ] Interrupt dispatching to drivers

- [x] IPC and messaging
  - [x] Buffered string messages
  - [x] Ports
  - [x] Kernel messages
  - [ ] Quicker messaging
  - [ ] Separate queues for channels

  
- [ ] Permissions
- [ ] Multi CPU support

**Core utilities and daemons**
- [ ] Process management (processd)
- [ ] Networking (networkd)
- [ ] Filesystems (filesystemsd)
  - [ ] FAT32
  - [ ] FUSE
- [ ] Drawing on screen (screend)
- [ ] Human input
- [ ] Native executables
- [ ] POSIX compatibility layer

**Drivers**
- [ ] PS/2
  - [ ] Keyboard
  - [ ] Mouse
- [ ] ATA/Bulk storage
- [ ] USB
- [ ] Serial/Parallel
- [ ] PCI/PCIe
- [ ] Network cards

**Userland**
- [ ] Terminal
- [ ] GUI
- [ ] C Library


## Architecture

The OS is based on a microkernel architecture, with the idea of running drivers and services in the userspace. The kernel currently has physical and virtual memory managers, scheduling, interrupts, IPC and permissions managed inside it. The processed are to be loaded and managed by the processd and everything else is planned to be had outside the kernel in the future.

The executables in ELF format are used

The system supports booting by the multiboot2 protocol. The loader is booted by the bootloader, which then prepares virtual memory as if it was an userspace program, loads the kernel (in elf format), briefly transfers execution to its init function, starts the processes via syscalls and then exits.

## License

_TODO_