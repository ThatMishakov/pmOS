EXEC = devicesd.elf

PROGDIR = $(realpath .)
ROOTDIR = $(PROGDIR)/..
export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

ARCH ?= x86_64

CC = clang
CPP = clang
CXX = clang++

TARGETFLAGS = --target=$(ARCH)-pmos --sysroot=$(ROOTDIR)/sysroot

USERCFLAGS = -g -Wall -Wextra -O3 -pipe -ggdb3 \
	-DUACPI_OVERRIDE_LIBC -flto -DUACPI_OVERRIDE_ARCH_HELPERS

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
	CCFILES = $(shell find ./arch/x86/ -type f -name '*.cc') $(shell find ./arch/x86_64/ -type f -name '*.cc')
	CFILES = $(shell find ./arch/x86/ -type f -name '*.c') $(shell find ./arch/x86_64/ -type f -name '*.c')
	ASMFILES = $(shell find ./arch/x86/ -type f -name '*.S') $(shell find ./arch/x86_64/ -type f -name '*.S')
	ARCHDIR = x86
# detect x86 and i286-i686
else ifneq ($(filter i%86 x86,$(ARCH)),)
	CCFILES = $(shell find ./arch/x86/ -type f -name '*.cc') $(shell find ./arch/i686/ -type f -name '*.cc')
	CFILES = $(shell find ./arch/x86/ -type f -name '*.c') $(shell find ./arch/i686/ -type f -name '*.c')
	ASMFILES = $(shell find ./arch/x86/ -type f -name '*.S') $(shell find ./arch/i686/ -type f -name '*.S')
	ARCHDIR = x86
	LDFLAGS += -lclang_rt.builtins-i386
else
	CCFILES = $(shell find ./arch/$(ARCH)/ -type f -name '*.cc')
	CFILES = $(shell find ./arch/$(ARCH)/ -type f -name '*.c')
	ASMFILES = $(shell find ./arch/$(ARCH)/ -type f -name '*.S')
	ARCHDIR = $(ARCH)
endif
 
# Use find to glob all *.c files in the directory and extract the object names.
CCFILES += $(shell find ./generic/ -type f -name '*.cc') $(shell find ./uACPI/source/ -type f -name '*.cc')
CFILES += $(shell find ./generic/ -type f -name '*.c') $(shell find ./uACPI/source/ -type f -name '*.c')
ASMFILES += $(shell find ./generic/ -type f -name '*.S') $(shell find ./uACPI/source/ -type f -name '*.S')


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
