EXEC = vfatfsd.elf

export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

CC = x86_64-pmos-gcc
CPP = x86_64-pmos-gcc
CXX = x86_64-pmos-g++
LD = x86_64-pmos-g++

INTERNALCFLAGS  :=       \
	-static

CFLAGS = -std=gnu17 $(INCLUDEFLAGS) -g
CXXFLAGS = -std=c++17 $(INCLUDEFLAGS) -g
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCLUDEFLAGS)
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS) -g


CCFILES := $(shell find ./ -type f -name '*.cc')
CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJ    := $(CCFILES:.cc=.o) $(ASMFILES:.S=.o) $(CFILES:.c=.o)
DEP    := $(OBJ:.o=.d)

all: $(EXEC)

$(EXEC): $(OBJ) libc
	$(CXX) $(LDFLAGS) $(OBJ) -lpthread -o $@

clean:
	$(RM) $(EXEC) $(OBJ) $(DEP)

-include $(DEP)

libc:
	$(MAKE) -C ../lib libc

.PHONY: ../lib/libpm.a