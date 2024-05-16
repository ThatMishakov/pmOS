EXEC = test.elf

export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

CC = riscv64-pmos-gcc
CPP = riscv64-pmos-gcc
CXX = riscv64-pmos-g++
LD = riscv64-pmos-gccgo
GO = riscv64-pmos-gccgo

INTERNALCFLAGS  :=       \
	-static

CFLAGS = -std=gnu20 $(INCLUDEFLAGS) -g
CXXFLAGS = -std=c++20 $(INCLUDEFLAGS) -g
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCLUDEFLAGS)
GOFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCLUDEFLAGS) -O3
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS) -g -pthread -O3 -static -Wl,--unresolved-symbols=ignore-in-object-files


CCFILES := $(shell find ./ -type f -name '*.cc')
CFILES := $(shell find ./ -type f -name '*.c')
GOFILES := $(shell find ./ -type f -name '*.go')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJ    := $(CCFILES:.cc=.o) $(ASMFILES:.S=.o) $(CFILES:.c=.o) $(GOFILES:.go=.o)
DEP    := $(OBJ:.o=.d)

all: $(EXEC)

%.o: %.go
	$(GO) $(GOFLAGS) -c $< -o $@

$(EXEC): $(OBJ) libc
	$(LD) $(LDFLAGS) $(OBJ) -o $@

clean:
	$(RM) $(EXEC) $(OBJ) $(DEP)

-include $(DEP)

libc:
	$(MAKE) -C ../lib libc

.PHONY: ../lib/libpm.a