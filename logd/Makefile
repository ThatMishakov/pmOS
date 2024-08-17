# Executable name
EXEC = logd.elf

ROOTDIR = $(realpath .)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH ?= x86_64

CC = $(ARCH)-pmos-gcc
CPP = $(ARCH)-pmos-gcc
CXX = $(ARCH)-pmos-g++

USERCFLAGS = -Wall -Wextra -O2 -pipe

CFLAGS = -std=c17 -g
CXXFLAGS = -std=c++20 -g
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS)

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=
 
# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-MMD -MP \
	-static \
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
