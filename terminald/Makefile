EXEC = terminald.elf

ROOTDIR = $(realpath .)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH ?= x86_64

CC = clang
CPP = clang
CXX = clang++

INCLUDEDIR =

USERCFLAGS = -Wall -Wextra -O3 -g -pipe -I.

TARGETFLAGS = --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot

CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP -DFLANTERM_FB_DISABLE_BUMP_ALLOC -DFLANTERM_FB_SUPPORT_BPP $(TARGETFLAGS)
CXXFLAGS =  -std=c++23
ASMFLAGS = -I$(INCLUDEDIR) $(TARGETFLAGS)
LDFLAGS = $(INTERNALLDFLAGS) $(TARGETFLAGS)

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS := -static

# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  := -static
 
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
	$(CXX) $(LDFLAGS) $(OBJ) -o $@
	
clean:
	$(RM) $(EXEC) $(OBJ)

-include $(DEP)

.PHONY: ../lib/libpm.a

libc:
	$(MAKE) -C ../lib libc

$(CC):
	$(MAKE) -C ../src x86_64-pmos-gcc
