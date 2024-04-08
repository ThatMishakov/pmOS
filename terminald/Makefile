EXEC = terminald.elf

ROOTDIR = $(realpath .)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH = x86_64

CC = $(ARCH)-pmos-gcc
CPP = $(ARCH)-pmos-gcc
CXX = $(ARCH)-pmos-g++

INCLUDEDIR =

USERCFLAGS = -Wall -Wextra -Og -g -pipe -I.

CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP -DFLANTERM_FB_DISABLE_BUMP_ALLOC -DFLANTERM_FB_SUPPORT_BPP
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS)

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS := -static

# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  := -static
 
# Use find to glob all *.c files in the directory and extract the object names.
CCFILES := $(shell find ./ -type f -name '*.cc')
CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJ    := $(CCFILES:.cc=.o) $(ASMFILES:.S=.o) $(CFILES:.c=.o)
DEP    := $(OBJ:.o=.d)
all: $(EXEC)

$(EXEC): $(OBJ) libc
	$(CC) $(LDFLAGS) $(OBJ) -o $@
	
clean:
	$(RM) $(EXEC) $(OBJ)

-include $(DEP)

.PHONY: ../lib/libpm.a

libc:
	$(MAKE) -C ../lib libc

$(CC):
	$(MAKE) -C ../src x86_64-pmos-gcc