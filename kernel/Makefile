# Kernel executable name
KERNEL = kernel

# It is highly recommended to use a custom built cross toolchain to build a kernel.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
CC = riscv64-pmos-gcc
CPP = riscv64-pmos-gcc
CXX = riscv64-pmos-g++
LD = riscv64-pmos-g++

ROOTDIR = $(realpath .)
INCLUDEDIR = include . libunwind/include arch/riscv64
INCPARAM = $(foreach d, $(INCLUDEDIR), -I$(ROOTDIR)/$d)

# User controllable CFLAGS.
USERCFLAGS = -Wall -Wextra -Og -pipe

CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCPARAM)
CXXFLAGS = -std=c++20
#CPPFLAGS = $(CFLAGS)
ASMFLAGS = $(INCPARAM)
LDFLAGS = $(INTERNALLDFLAGS)

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=     \
	-nostdlib \
	-ffreestanding \
	-static \
	-Tlinker.ld    \
	-fPIC \
	-lgcc \
	-no-pie -fno-pic -Wl,--demangle
 
# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-static \
	-ffreestanding \
	-fexceptions \
	-no-pie -fno-pic \
	-D_LIBUNWIND_IS_BAREMETAL \
	-fasynchronous-unwind-tables \
	-march=rv64ima_zicsr_zba_zbb_zbc -mabi=lp64 \
	-Wall -Wextra \
	-g
 
# Use find to glob all *.c files in the directory and extract the object names.
CCFILES := $(shell find ./ -type f -name '*.cc')
CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJS = 	./objects/icxxabi.o ./utils.o types.o \
	    ./memory/mem.o ./memory/mem_regions.o ./memory/malloc.o ./memory/dlmalloc.o ./memory/palloc.o ./memory/paging.o \
		./memory/temp_mapper.o ./memory/mem_object.o ./memory/page_descriptor.o ./memory/virtmem.o \
	    ./processes/idle.o ./processes/syscalls.o ./processes/tasks.o ./processes/task_group.o \
		./sched/sched.o ./sched/sched_queue.o \
		./messaging/messaging.o ./messaging/named_ports.o \
		./kern_logger/kern_logger.o \
		./limine/limine.o \
		\
		./arch/riscv64/cpus/cpus_asm.o ./arch/riscv64/tasks/task_switch.o ./arch/riscv64/arch_specific.o \
		./arch/riscv64/sbi/console.o \
		./arch/riscv64/cpus/sched_helpers.o \
		./arch/riscv64/start.o ./arch/riscv64/console.o \
		\
		./libunwind/src/libunwind.o ./libunwind/src/Unwind-seh.o ./libunwind/src/Unwind-sjlj.o ./libunwind/src/UnwindLevel1.o \
		./libunwind/src/UnwindLevel1-gcc-ext.o ./libunwind/src/UnwindRegistersRestore.o ./libunwind/src/UnwindRegistersSave.o \
		./libcxxrt/src/cxa_atexit.o ./libcxxrt/src/cxa_finalize.o \
		./libcxxrt/src/dynamic_cast.o ./libcxxrt/src/guard.o \
		./libcxxrt/src/memory.o ./libcxxrt/src/stdexcept.o \
		./libcxxrt/src/terminate.o ./libcxxrt/src/typeinfo.o \
		./libcxxrt/src/auxhelper.o ./libcxxrt/src/exception.o

# CRTI_OBJ =  ./objects/crti.o
# CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
# CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
# CRTN_OBJ =  ./objects/crtn.o

# OBJ_LINK_LIST:=$(CRTI_OBJ) $(CRTBEGIN_OBJ) $(OBJS) $(CRTEND_OBJ) $(CRTN_OBJ)
OBJ_LINK_LIST:=$(OBJS)
INTERNAL_OBJS = $(CRTI_OBJ) $(OBJS) $(CRTN_OBJ) 
DEP    := $(INTERNAL_OBJS:.o=.d)

$(KERNEL): $(OBJ_LINK_LIST) linker.ld
	$(LD) $(LDFLAGS) $(OBJ_LINK_LIST) -o $@
	
clean:
	$(RM) $(KERNEL) $(INTERNAL_OBJS) $(DEP)

-include $(DEP)

