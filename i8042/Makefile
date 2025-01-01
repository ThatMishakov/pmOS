# Kernel executable name
EXEC = i8042.elf

ROOTDIR = $(realpath .)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH ?= x86_64

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