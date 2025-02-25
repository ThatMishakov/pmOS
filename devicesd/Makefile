EXEC = devicesd.elf

PROGDIR = $(realpath .)
ROOTDIR = $(PROGDIR)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH ?= x86_64

CC = clang
CPP = clang
CXX = clang++

TARGETFLAGS = --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot

USERCFLAGS = -Wall -Wextra -O3 -pipe -ggdb3 \
	-DUACPI_OVERRIDE_LIBC -flto

INCLUDEFLAGS = -I$(PROGDIR)/include -I$(PROGDIR)/uACPI/include

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=     \
	-static -flto

# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-static \
	-fno-exceptions

CFLAGS = -std=gnu23 $(INCLUDEFLAGS) -g $(TARGETFLAGS)
CXXFLAGS = -std=c++23 $(INCLUDEFLAGS) -g $(TARGETFLAGS)
CPPFLAGS = $(INTERNALCFLAGS) $(USERCFLAGS) -MMD -MP $(INCLUDEFLAGS) $(TARGETFLAGS)
ASMFLAGS = -I$(INCLUDEDIR) $(TARGETFLAGS)
LDFLAGS = $(INTERNALLDFLAGS) -g $(TARGETFLAGS)

ifeq ($(ARCH), x86_64)
	ARCHDIR = x86
# detect x86 and i286-i686
else ifneq ($(filter i%86 x86,$(ARCH)),)
	ARCHDIR = x86
	LDFLAGS += -lclang_rt.builtins-i386
else
	ARCHDIR = $(ARCH)
endif
 
# Use find to glob all *.c files in the directory and extract the object names.
CCFILES := $(shell find ./generic/ -type f -name '*.cc') $(shell find ./arch/$(ARCHDIR)/ -type f -name '*.cc') $(shell find ./uACPI/source/ -type f -name '*.cc')
CFILES := $(shell find ./generic/ -type f -name '*.c') $(shell find ./arch/$(ARCHDIR)/ -type f -name '*.c') $(shell find ./uACPI/source/ -type f -name '*.c')
ASMFILES := $(shell find ./generic/ -type f -name '*.S') $(shell find ./arch/$(ARCHDIR)/ -type f -name '*.S') $(shell find ./uACPI/source/ -type f -name '*.S')


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
