# Executable name
EXEC = ns16550.elf

ROOTDIR = $(realpath .)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH ?= x86_64

CC = clang
CPP = clang
CXX = clang++

TARGETFLAGS = --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot

USERCFLAGS = -Wall -Wextra -O2 -pipe


CFLAGS = -std=c17 -g $(TARGETFLAGS)
CXXFLAGS = -std=c++23 -g $(TARGETFLAGS)
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(TARGETFLAGS)
ASMFLAGS = -I$(INCLUDEDIR) $(TARGETFLAGS)
LDFLAGS = $(INTERNALLDFLAGS) $(TARGETFLAGS)

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=
 
# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-MMD -MP \
	-static \
	-fPIE
 
ifeq ($(ARCH), riscv64)
	INTERNALCFLAGS += -march=rva20u64 -mabi=lp64d
else ifneq ($(filter i%86 x86,$(ARCH)),)
	LDFLAGS += -lclang_rt.builtins-i386
endif

# Use find to glob all *.c files in the directory and extract the object names.
CCFILES := $(shell find ./ -type f -name '*.cc')
CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJ    := $(CCFILES:.cc=.o) $(ASMFILES:.S=.o) $(CFILES:.c=.o)
DEP    := $(OBJ:.o=.d)
all: $(EXEC)

$(EXEC): $(OBJ) libc
	$(CXX) $(LDFLAGS) $(OBJ) -o $@ -static
	
clean:
	$(RM) $(EXEC) $(OBJ) $(DEP)

-include $(DEP)

libc:
	$(MAKE) -C ../lib libc

$(CC):
	$(MAKE) -C ../src x86_64-pmos-gcc

.PHONY: libc
