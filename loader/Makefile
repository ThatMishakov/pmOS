# Executable name
EXEC = loader.elf

ROOTDIR = $(realpath .)
CC = $(ROOTDIR)/../bin/cross/bin/x86_64-pmos-gcc
CPP = $(ROOTDIR)/../bin/cross/bin/x86_64-pmos-gcc
CXX = $(ROOTDIR)/../bin/cross/bin/x86_64-pmos-g++
LD = $(ROOTDIR)/../bin/cross/bin/x86_64-pmos-g++

INCLUDEDIR = include

# User controllable CFLAGS.
USERCFLAGS = -Wall -Wextra -O2 -pipe

CPPFLAGS = $(INTERNALCFLAGS) -I$(INCLUDEDIR) $(USERCFLAGS) -I../kernel/include  -MMD -MP
#CXXFLAGS = $(CFLAGS)
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS)

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS := \
    -nostdlib               \
	-Tlinker.ld    \
	-z max-page-size=0x1000 \
	-static
 
# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  := \
	-I.					 \
	-ffreestanding       \
	-fno-stack-protector \
	-mno-80387           \
	-mno-mmx             \
	-mno-3dnow           \
	-mno-sse             \
	-mno-sse2            \
	-mno-red-zone
 
# Use find to glob all *.c files in the directory and extract the object names.
CCFILES := $(shell find ./ -type f -name '*.cc')
CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJ    := $(CCFILES:.cc=.o) $(ASMFILES:.S=.o) $(CFILES:.c=.o)

$(EXEC): $(OBJ) linker.ld
	$(LD) $(LDFLAGS) $(OBJ) -o $@

	
clean:
	rm -f $(EXEC) *.o *.s *.d

all: $(EXEC)

-include $(DEP)
