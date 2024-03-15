# Kernel executable name
EXEC = vfsd.elf

ROOTDIR = $(realpath .)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

# It is highly recommended to use a custom built cross toolchain to build a kernel.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
CC = riscv64-pmos-gcc
CPP = riscv64-pmos-gcc
CXX = riscv64-pmos-g++

# User controllable CFLAGS.
USERCFLAGS = -Wall -Wextra -Og -g -pipe

CFLAGS = -std=c17 -g
CXXFLAGS = -std=c++17 -g
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS)

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=
 
# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-MMD -MP \
	-static \
	-fPIE
 
# Use find to glob all *.c files in the directory and extract the object names.
CCFILES := $(shell find ./ -type f -name '*.cc')
CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJ    := $(CCFILES:.cc=.o) $(ASMFILES:.S=.o) $(CFILES:.c=.o)
DEP    := $(OBJ:.o=.d)
all: $(EXEC)

$(EXEC): $(OBJ) libc
	$(CC) $(LDFLAGS) $(OBJ) -o $@
	
clean:
	$(RM) $(EXEC) $(OBJ) $(DEP)

-include $(DEP)

libc:
	$(MAKE) -C ../lib libc

$(CC):
	$(MAKE) -C ../src x86_64-pmos-gcc

.PHONY: libc