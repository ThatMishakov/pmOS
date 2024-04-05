# pmOS


A hobbyist operating system for x86_64 and RISC-V written in C, C++ and ASM. The goal of the project is to learn about the OS development and ideally to develop a complex system with GUI and different conveniences you would expect out of one.

## Compilation and execution

TODO

Compile gcc-pmos and binutils-gdb-pmos and everything else with it
The build system is broken after the recent changes and I compile everything manually

## Features

These are the features that are planned to be had in the OS:

### Kernel

#### Arch-independent features:
- [ ] Processes and threads
  - [ ] Processes - The kernel has tasks, but POSIX processes and execlp/spawn is not yet implemented
  - [x] Task switching
  - [x] Preemptive multitasking
  - [X] Threads - basic pthread implementation in userspace building on kernel interfaces
  - [X] Kernel threads
  - [x] User space

- [ ] Memory
  - [x] Page frame allocator
  - [x] Kernel virtual memory manager
  - [x] kmalloc
  - [x] Allocating memory to userspace
  - [ ] Userspace memory regions
    - [x] Mapping (anonymous mmap)
    - [x] Lazy allocations
    - [x] Full unmapping
    - [ ] Partial and overlaping unmapping (munmap)
    - [ ] memprotect and changing permissions
    - [x] Acccess to physical memory
    - [x] Mapping of memory objects
  - [x] Memory mapping
  - [x] Releasing used pages
  - [ ] Memory objects
    - [x] Memory object creation from kernel
    - [x] Memory object access from kernel
    - [ ] Blocking tasks on memory object access in kernel
      - Need to make changes in scheduler
    - [x] Memory object mapping to userspace
    - [ ] Memory object creation and management from userspace
  - [x] Delayed allocation
  - [x] Memory protections
  - [ ] Swapping
  - [x] Accessing userspace memory
    - Implemented (and surprisingly works well), but is very slow, needs rewriting

- [ ] Interrupts and exceptions
  - [x] Very basic exception handling
  - [x] Syscalls
  - [ ] Interrupt dispatching to drivers
    - I had something working on x86, but it needs rewriting and doesn't work on RISC-V at all

- [x] IPC and messaging
  - [x] Buffered string messages
  - [x] Ports
  - [x] Kernel messages
  - [ ] Quicker messaging
  
- [ ] Permissions
- [ ] Multi CPU support
  Most of the kernel should work (and was working at some point), but memory unmapping among thread needs to be rewritten

#### RISC-V specific features:

- [x] Virtual memory
- [x] Exceptions
- [x] Timer interrupt
- [x] Userspace/U mode
- [ ] Multi hart support

#### x86_64 specific features:

- [x] Virtual memory
- [x] Exceptions
- [ ] Timer interrupt
  The infrastructure for it is there, but it's seemingly broken, after switching to limine
- [x] LAPIC
  Untested, but was working before
- [x] Userspace/Ring 3
- [ ] Multi CPU support
  Was working at some point, but needs rewriting after switching to limine + paging and threads need to be fixed



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
  - [x] Framebuffer
    - [x] Showing text with framebuffer
- [ ] Human input
- [X] Native executables
- [ ] POSIX compatibility layer - Many functions are implemented and working, but a lot more are yet to be done

**Drivers**
- [X] PS/2
  - [X] Keyboard
  - [ ] Mouse
- [x] i8042
  - Probably works, but it's untested and disabled after recent changes
- [ ] ATA/Bulk storage
- [ ] USB
- [ ] Serial/Parallel
- [ ] PCI/PCIe
- [ ] Network cards
- [ ] Timers
  - [x] HPET
    Untested, was working before
- [ ] virtio
  - [ ] virtio-user-input


**Userland**
- [ ] Terminal
- [ ] GUI
- [ ] C Library
  TODO: Enumerate what's done. Very incomplete, but enough stuff is implemented to compile GCC C++ library and stuff

**Languages/Runtimes**
- [X] Hosted GCC cross-compiler
- [X] C and its runtime (using homemade C/POSIX library)
- [X] C++ and libstdc++ (and other) runtime(s) - Compiling and working but a lot of C library functions are missing
- [X] Go and libgo runtime - Compiling, but untested and probably broken for now. Also, exec-stuff is missing


## Architecture

The OS is based on a microkernel architecture, with the idea of running drivers and services in the userspace. The kernel currently has physical and virtual memory managers, scheduling, interrupts, IPC and permissions managed inside it. Of the process management, the kernel only knows about tasks, with (POSIX) processes and threads being an
abstraction on top of it, being managed in a userspace

The executables in ELF format are used

The system supports booting by the limine protocol. In the past, multiboot2 was used with a stage 2 bootloader, but it was replaced by limine, since it works on both x86_64 and RISC-V. The kernel is loaded by limine, which initializes itself and starts the first user space task from a module called "bootstrap". It then recieves some info from the kernel and list of modules, moved to memory objects and starts those up, and maps itself as a root filesystem.

## Known issues

## Documentation

In works!

## Dependencies

The following libraries have been used in the project, possibly containing their own licenses:
- [libunwind](/kernel/libunwind/)
- [libcxxrt](/kernel/libcxxrt/)
- [smoldtb](/kernel/smoldtb/)
- [flanterm](/terminald/flanterm/)
- dlmalloc in libc and kernel

## License

Unless otherwise stated (see the dependencies), the project is licensed under the BSD 3-Clause license. See the [LICENSE](LICENSE) file for more information.