# Kernel executable name
KERNEL = kernel

# It is highly recommended to use a custom built cross toolchain to build a kernel.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
CC = x86_64-elf-gcc
CPP = x86_64-elf-gcc
CXX = x86_64-elf-g++
LD = x86_64-elf-g++

INCLUDEDIR = include

# User controllable CFLAGS.
USERCFLAGS = -Wall -Wextra -O3 -pipe

CPPFLAGS = $(INTERNALCFLAGS) -I$(INCLUDEDIR) -mcmodel=large $(USERCFLAGS) -MMD -MP
CXXFLAGS = -std=c++20
#CPPFLAGS = $(CFLAGS)
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS) -mcmodel=large

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=     \
	-nostdlib \
	-lgcc \
	-ffreestanding \
	-static \
	-Tlinker.ld    \
	-fPIC \
	-no-pie -fno-pic
 
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
#OBJ    := $(CCFILES:.cc=.o) $(ASMFILES:.S=.o) $(CFILES:.c=.o) 
OBJ := ./malloc.o ./exceptions_managers.o ./free_page_alloc.o ./gdt.o ./idle.o ./interrupts.o ./main.o ./mem.o ./messaging.o ./misc.o ./paging.o ./palloc.o ./sched.o ./syscalls.o ./utils.o ./loadGDT.o ./start.o ./interrupts_asm.o ./asm.o
DEP    := $(OBJ:.o=.d)

$(KERNEL): $(OBJ) linker.ld
	$(LD) $(LDFLAGS) $(OBJ) -o $@
	
clean:
	$(RM) $(KERNEL) $(OBJ) $(DEP)

-include $(DEP)
