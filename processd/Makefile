# Kernel executable name
EXEC = processd.elf

ROOTDIR = $(realpath .)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

# It is highly recommended to use a custom built cross toolchain to build a kernel.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
CC = x86_64-pmos-gcc
CPP = x86_64-pmos-gcc
CXX = x86_64-pmos-g++

# User controllable CFLAGS.
USERCFLAGS = -Wall -Wextra -O2 -pipe

CFLAGS = -std=c17
CXXFLAGS = -std=c++17
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS)

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=     \
	-static \
	-z max-page-size=0x1000
 
# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-static \
	-fPIE \
	-fno-exceptions -fno-rtti
 
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
	$(RM) $(EXEC) $(OBJ)

-include $(DEP)

libc:
	$(MAKE) -C ../lib libc

$(CC):
	$(MAKE) -C ../src x86_64-pmos-gcc

.PHONY: ../lib/libpm.a
