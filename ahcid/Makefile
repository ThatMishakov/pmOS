# Executable name
EXEC = ahcid.elf

ROOTDIR = $(realpath .)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH ?= x86_64

CC = clang
CPP = clang
CXX = clang++

USERCFLAGS = -Wall -Wextra -Og -pipe

CFLAGS = -std=c23 -g --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot -MMD -MP
CXXFLAGS = -std=c++23 -g --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot -MMD -MP
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS)
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS) --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot -static

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=
 
# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-fPIE -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer

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

.PHONY: libc
