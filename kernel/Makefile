# Kernel executable name
KERNEL = kernel

# It is highly recommended to use a custom built cross toolchain to build a kernel.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
CC = x86_64-elf-gcc
CPP = x86_64-elf-gcc
CXX = x86_64-elf-g++
LD = x86_64-elf-g++

ROOTDIR = $(realpath .)
INCLUDEDIR = include .
INCPARAM = $(foreach d, $(INCLUDEDIR), -I$(ROOTDIR)/$d)

# User controllable CFLAGS.
USERCFLAGS = -Wall -Wextra -Og -pipe

CPPFLAGS = $(INTERNALCFLAGS) -mcmodel=large $(USERCFLAGS) -MMD -MP $(INCPARAM)
CXXFLAGS = -std=c++20 $(INCPARAM)
#CPPFLAGS = $(CFLAGS)
ASMFLAGS = $(INCPARAM)
LDFLAGS = $(INTERNALLDFLAGS) -mcmodel=large

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=     \
	-nostdlib \
	-lgcc \
	-ffreestanding \
	-static \
	-Tlinker.ld    \
	-fPIC \
	-no-pie -fno-pic -Wl,--demangle
 
# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-static \
	-ffreestanding \
	-mno-red-zone \
	-mno-mmx -mno-sse -mno-sse2 \
	-no-pie -fno-pic \
	-fno-exceptions -fno-rtti \
 
# Use find to glob all *.c files in the directory and extract the object names.
CCFILES := $(shell find ./ -type f -name '*.cc')
CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJS = 	./objects/icxxabi.o ./start.o ./asm.o ./main.o ./misc.o ./utils.o \
	    ./memory/mem.o ./memory/malloc.o ./memory/free_page_alloc.o ./memory/palloc.o ./memory/paging.o ./memory/shared_mem.o \
	    ./interrupts/apic.o ./interrupts/pic.o ./interrupts/gdt.o ./interrupts/exceptions_managers.o ./interrupts/interrupts.o \
		./interrupts/pit.o ./interrupts/interrupts_asm.o ./interrupts/loadGDT.o\
	    ./processes/idle.o ./processes/syscalls.o ./processes/tasks.o ./processes/syscalls_asm.o \
		./sched/sched.o ./sched/timers.o \
		./messaging/messaging.o ./cpus/cpus.o

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
