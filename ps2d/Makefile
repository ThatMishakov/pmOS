# Kernel executable name
EXEC = ps2d.elf

ROOTDIR = $(realpath .)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH ?= x86_64

# It is highly recommended to use a custom built cross toolchain to build a kernel.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
CC = clang
CPP = clang
CXX = clang++

TARGETFLAGS = --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot

# User controllable CFLAGS.
USERCFLAGS = -Wall -Wextra -O3 -pipe -flto

CFLAGS = $(TARGETFLAGS) -std=c17
CXXFLAGS = $(TARGETFLAGS) -std=c++17
CPPFLAGS = $(TARGETFLAGS) $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP
ASMFLAGS = $(TARGETFLAGS) -I$(INCLUDEDIR)
LDFLAGS = $(TARGETFLAGS) $(INTERNALLDFLAGS)

ifneq ($(filter i%86 x86,$(ARCH)),)
	LDFLAGS += -lclang_rt.builtins-i386
endif

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