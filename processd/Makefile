# Kernel executable name
EXEC = processd.elf

# It is highly recommended to use a custom built cross toolchain to build a kernel.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
CC = x86_64-elf-gcc
CPP = x86_64-elf-gcc
CXX = x86_64-elf-g++
LD = x86_64-elf-gcc

INCLUDEDIR = include:../kernel/include:../lib/include

# User controllable CFLAGS.
USERCFLAGS = -Wall -Wextra -O2 -pipe

CFLAGS = -std=c17
CXXFLAGS = -std=c++17
CPPFLAGS = $(INTERNALCFLAGS) -Iinclude -I../kernel/include -I../lib/include $(USERCFLAGS) -MMD -MP
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS)

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=     \
	-ffreestanding \
	-nostdlib      \
	-static \
	-Tlinker.ld    \
	-z max-page-size=0x1000
 
# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-ffreestanding \
	-static \
	-mno-red-zone \
	-mno-mmx -mno-sse -mno-sse2 \
	-fPIE \
	-fno-exceptions -fno-rtti
 
# Use find to glob all *.c files in the directory and extract the object names.
CCFILES := $(shell find ./ -type f -name '*.cc')
CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJ    := $(CCFILES:.cc=.o) $(ASMFILES:.S=.o) $(CFILES:.c=.o)
DEP    := $(OBJ:.o=.d)
all: $(EXEC)

AFTER_EXEC = -L../lib/ -l pm

$(EXEC): $(OBJ) ../lib/libpm.a linker.ld
	$(LD) $(LDFLAGS) $(OBJ) $(AFTER_EXEC) -o $@
	
clean:
	$(RM) $(EXEC) $(OBJ)

-include $(DEP)

../lib/libpm.a:
	$(MAKE) -C ../lib libpm.a

.PHONY: ../lib/libpm.a