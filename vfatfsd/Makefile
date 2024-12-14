EXEC = vfatfsd.elf

ROOTDIR = $(realpath .)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH ?= x86_64

CC = clang
CPP = clang
CXX = clang++
LD = clang++

TARGETFLAGS = --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot

INTERNALCFLAGS  :=       \
	-static

#USERCFLAGS := -march=rv64gc -mabi=lp64d -O3
USERCFLAGS := -O3 -flto

CFLAGS = -std=gnu2x $(INCLUDEFLAGS) -g $(TARGETFLAGS)
CXXFLAGS = -std=c++20 $(INCLUDEFLAGS) -g $(TARGETFLAGS)
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) $(INCLUDEFLAGS) $(TARGETFLAGS)
GOFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) $(INCLUDEFLAGS) $(TARGETFLAGS)
ASMFLAGS = -I$(INCLUDEDIR) $(TARGETFLAGS)
LDFLAGS = $(INTERNALLDFLAGS) -static -g -O3 -flto $(TARGETFLAGS) # -march=rv64gc -mabi=lp64d$

ifeq ($(ARCH), riscv64)
	INTERNALCFLAGS += -march=rva20u64 -mabi=lp64d
else ifneq ($(filter i%86 x86,$(ARCH)),)
	LDFLAGS += -lclang_rt.builtins-i386
endif

CCFILES := $(shell find ./ -type f -name '*.cc')
CFILES := $(shell find ./ -type f -name '*.c')
GOFILES := $(shell find ./ -type f -name '*.go')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJ    := $(CCFILES:.cc=.o) $(ASMFILES:.S=.o) $(CFILES:.c=.o) $(GOFILES:.go=.o)
DEP    := $(OBJ:.o=.d)

all: $(EXEC)

$(EXEC): $(OBJ) libc
	$(LD) $(LDFLAGS) $(OBJ) -o $@

clean:
	$(RM) $(EXEC) $(OBJ) $(DEP)

-include $(DEP)

libc:
	$(MAKE) -C ../lib libc

.PHONY: ../lib/libpm.a
