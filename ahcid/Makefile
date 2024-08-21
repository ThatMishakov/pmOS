# Executable name
EXEC = ahcid.elf

ROOTDIR = $(realpath .)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH ?= x86_64

CC = clang
CPP = clang
CXX = clang++

USERCFLAGS = -Wall -Wextra -O2 -pipe

CFLAGS = -std=c17 -g --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot
CXXFLAGS = -std=c++20 -g --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS)
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS) --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot -static

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=
 
# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-fPIE
 
# Use find to glob all *.c files in the directory and extract the object names.
CCFILES := $(shell find ./ -type f -name '*.cc')
CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJ    := $(CCFILES:.cc=.o) $(ASMFILES:.S=.o) $(CFILES:.c=.o)
DEP    := $(OBJ:.o=.d)
all: $(EXEC)

$(EXEC): $(OBJ) libc
	$(CXX) $(LDFLAGS) $(OBJ) -o $@ -pthread -static
	
clean:
	$(RM) $(EXEC) $(OBJ) $(DEP)

-include $(DEP)

libc:
	$(MAKE) -C ../lib libc

$(CC):
	$(MAKE) -C ../src x86_64-pmos-gcc

.PHONY: libc
