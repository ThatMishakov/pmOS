EXEC = vfatfsd.elf

export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH = riscv64

CC = $(ARCH)-pmos-gcc
CPP = $(ARCH)-pmos-gcc
CXX = $(ARCH)-pmos-g++
LD = $(ARCH)-pmos-g++
GO = $(ARCH)-pmos-gccgo

INTERNALCFLAGS  :=       \
	-static

#USERCFLAGS := -march=rv64gc -mabi=lp64d -O3
USERCFLAGS := -O3

CFLAGS = -std=gnu2x $(INCLUDEFLAGS) -g
CXXFLAGS = -std=c++20 $(INCLUDEFLAGS) -g
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCLUDEFLAGS)
GOFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCLUDEFLAGS)
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS) -static -g -pthread -O3 # -march=rv64gc -mabi=lp64d


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
