EXEC = test.elf

export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

CC = x86_64-pmos-gcc
CPP = x86_64-pmos-gcc
CXX = x86_64-pmos-g++
LD = x86_64-pmos-gccgo
GO = x86_64-pmos-gccgo

INTERNALCFLAGS  :=       \
	-static

CFLAGS = -std=gnu20 $(INCLUDEFLAGS) -g
CXXFLAGS = -std=c++20 $(INCLUDEFLAGS) -g
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCLUDEFLAGS)
GOFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCLUDEFLAGS) -O3
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS) -g -pthread -O3


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