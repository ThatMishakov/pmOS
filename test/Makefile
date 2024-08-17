EXEC = test.elf

export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH ?= x86_64

CC = $(ARCH)-pmos-gcc
CPP = $(ARCH)-pmos-gcc
CXX = $(ARCH)-pmos-g++
LD = $(ARCH)-pmos-gccgo
GO = $(ARCH)-pmos-gccgo

INTERNALCFLAGS  :=       \
	-static

CFLAGS = -std=gnu20 $(INCLUDEFLAGS) -g
CXXFLAGS = -std=c++20 $(INCLUDEFLAGS) -g
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCLUDEFLAGS)
GOFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCLUDEFLAGS) -O3
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS) -g -pthread -O1 -static -Wl,--unresolved-symbols=ignore-in-object-files


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
