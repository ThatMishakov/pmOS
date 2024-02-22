# Kernel executable name
KERNEL = kernel

# It is highly recommended to use a custom built cross toolchain to build a kernel.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
CC = x86_64-pmos-gcc
CPP = x86_64-pmos-gcc
CXX = x86_64-pmos-g++
LD = x86_64-pmos-g++

ROOTDIR = $(realpath .)
INCLUDEDIR = include . libunwind/include minidbg/x86_64
INCPARAM = $(foreach d, $(INCLUDEDIR), -I$(ROOTDIR)/$d)

# User controllable CFLAGS.
USERCFLAGS = -Wall -Wextra -O3 -pipe

CPPFLAGS = $(INTERNALCFLAGS) -mcmodel=kernel $(USERCFLAGS) -MMD -MP $(INCPARAM)
CXXFLAGS = -std=c++20
#CPPFLAGS = $(CFLAGS)
ASMFLAGS = $(INCPARAM)
LDFLAGS = $(INTERNALLDFLAGS) -mcmodel=kernel

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=     \
	-nostdlib \
	-ffreestanding \
	-static \
	-Tlinker.ld    \
	-fPIC \
	-no-pie -fno-pic -Wl,--demangle
 
# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-static \
	-ffreestanding \
	-fexceptions \
	-mno-red-zone \
	-mno-80387 \
	-msoft-float \
	-mno-fp-ret-in-387 \
	-no-pie -fno-pic \
	-D_LIBUNWIND_IS_BAREMETAL \
	-mgeneral-regs-only \
	-fasynchronous-unwind-tables \
	-g
 
# Use find to glob all *.c files in the directory and extract the object names.
CCFILES := $(shell find ./ -type f -name '*.cc')
CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJS = 	./objects/icxxabi.o ./start.o ./asm.o ./main.o ./misc.o ./utils.o types.o \
	    ./memory/mem.o ./memory/mem_regions.o ./memory/malloc.o ./memory/dlmalloc.o ./memory/free_page_alloc.o ./memory/palloc.o ./memory/paging.o \
		./memory/temp_mapper.o ./memory/mem_object.o ./memory/page_descriptor.o \
	    ./interrupts/apic.o ./interrupts/pic.o ./interrupts/gdt.o ./interrupts/exceptions_managers_asm.o ./interrupts/exceptions_managers.o \
		./interrupts/interrupts.o ./interrupts/asm_helpers.o ./interrupts/programmable_ints.o \
		./interrupts/pit.o ./interrupts/interrupts_asm.o ./interrupts/loadGDT.o ./interrupts/idt.o \
	    ./processes/idle.o ./processes/syscalls.o ./processes/tasks.o ./processes/task_group.o ./processes/syscalls_asm.o \
		./sched/sched.o ./sched/timers.o ./sched/sched_queue.o \
		./messaging/messaging.o ./messaging/named_ports.o \
		./cpus/cpus.o ./cpus/sse.o ./cpus/ipi.o ./cpus/ipi_asm.o \
		./kern_logger/kern_logger.o \
		./limine/limine.o ./limine/limie_entry.o \
		\
		./libunwind/src/libunwind.o ./libunwind/src/Unwind-seh.o ./libunwind/src/Unwind-sjlj.o ./libunwind/src/UnwindLevel1.o \
		./libunwind/src/UnwindLevel1-gcc-ext.o ./libunwind/src/UnwindRegistersRestore.o ./libunwind/src/UnwindRegistersSave.o \
		./libcxxrt/src/cxa_atexit.o ./libcxxrt/src/cxa_finalize.o \
		./libcxxrt/src/dynamic_cast.o ./libcxxrt/src/guard.o \
		./libcxxrt/src/memory.o ./libcxxrt/src/stdexcept.o \
		./libcxxrt/src/terminate.o ./libcxxrt/src/typeinfo.o \
		./libcxxrt/src/auxhelper.o ./libcxxrt/src/exception.o \
		./minidbg/dbg.o ./minidbg/x86_64/dbga.o ./minidbg/x86_64/dbgc.o ./minidbg/x86_64/uart.o

CRTI_OBJ =  ./objects/crti.o
CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ =  ./objects/crtn.o

OBJ_LINK_LIST:=$(CRTI_OBJ) $(CRTBEGIN_OBJ) $(OBJS) $(CRTEND_OBJ) $(CRTN_OBJ)
INTERNAL_OBJS = $(CRTI_OBJ) $(OBJS) $(CRTN_OBJ) 
DEP    := $(INTERNAL_OBJS:.o=.d)

$(KERNEL): $(OBJ_LINK_LIST) linker.ld
	$(LD) $(LDFLAGS) $(OBJ_LINK_LIST) -o $@
	
clean:
	$(RM) $(KERNEL) $(INTERNAL_OBJS) $(DEP)

-include $(DEP)

