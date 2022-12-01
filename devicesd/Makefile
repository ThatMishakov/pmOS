EXEC = devicesd.elf

PROGDIR = $(realpath .)
ROOTDIR = $(PROGDIR)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

CC = $(ROOTDIR)/bin/cross/bin/x86_64-pmos-gcc
CPP = $(ROOTDIR)/bin/cross/bin/x86_64-pmos-gcc
CXX = $(ROOTDIR)/bin/cross/bin/x86_64-pmos-g++

USERCFLAGS = -Wall -Wextra -O2 -pipe

INCLUDEFLAGS = -I$(PROGDIR)/include

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=     \
	-static

# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-static \
	-mno-red-zone \
	-mno-mmx -mno-sse -mno-sse2 \
	-fPIE \
	-fno-exceptions

CFLAGS = -std=gnu17 $(INCLUDEFLAGS) -g
CXXFLAGS = -std=c++17 $(INCLUDEFLAGS) -g
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCLUDEFLAGS)
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS) -g
 
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
	$(RM) $(EXEC) $(OBJ) $(DEP)

-include $(DEP)

libc:
	$(MAKE) -C ../lib libc

.PHONY: ../lib/libpm.a