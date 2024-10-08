EXEC = devicesd.elf

PROGDIR = $(realpath .)
ROOTDIR = $(PROGDIR)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH ?= x86_64

CC = clang
CPP = clang
CXX = clang++

TARGETFLAGS = --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot

USERCFLAGS = -Wall -Wextra -O2 -pipe

INCLUDEFLAGS = -I$(PROGDIR)/include

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=     \
	-static

# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-static \
	-fno-exceptions

CFLAGS = -std=gnu17 $(INCLUDEFLAGS) -g $(TARGETFLAGS)
CXXFLAGS = -std=c++20 $(INCLUDEFLAGS) -g $(TARGETFLAGS)
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCLUDEFLAGS) $(TARGETFLAGS)
ASMFLAGS = -I$(INCLUDEDIR) $(TARGETFLAGS)
LDFLAGS = $(INTERNALLDFLAGS) -g $(TARGETFLAGS)
 
# Use find to glob all *.c files in the directory and extract the object names.
CCFILES := $(shell find ./generic/ -type f -name '*.cc') $(shell find ./arch/$(ARCH)/ -type f -name '*.cc') 
CFILES := $(shell find ./generic/ -type f -name '*.c') $(shell find ./arch/$(ARCH)/ -type f -name '*.c') 
ASMFILES := $(shell find ./generic/ -type f -name '*.S') $(shell find ./arch/$(ARCH)/ -type f -name '*.S') 
OBJ    := $(CCFILES:.cc=.o) $(ASMFILES:.S=.o) $(CFILES:.c=.o)
DEP    := $(OBJ:.o=.d)
all: $(EXEC)

$(EXEC): $(OBJ) libc
	$(CC) $(LDFLAGS) $(OBJ) -o $@

clean:
	$(RM) $(EXEC) $(OBJ) $(DEP)

-include $(DEP)

libc:
	$(MAKE) -C ../lib libc

.PHONY: ../lib/libpm.a
