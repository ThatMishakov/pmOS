# Kernel executable name
EXEC = processd.elf

# It is highly recommended to use a custom built cross toolchain to build a kernel.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
CC = clang
CPP = clang
CXX = clang++
TARGET := x86_64-elf

INCLUDEDIR = include

# User controllable CFLAGS.
USERCFLAGS = -Wall -Wextra -O2 -pipe

CPPFLAGS = $(INTERNALCFLAGS) -I$(INCLUDEDIR) $(USERCFLAGS) -MMD -MP
ASMFLAGS = -I$(INCLUDEDIR)
LDFLAGS = $(INTERNALLDFLAGS) -target $(TARGET)

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=     \
	-ffreestanding \
	-nostdlib      \
	-static \
	-fuse-ld=lld \
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

$(EXEC): $(OBJ) # linker.ld
	$(CC) $(LDFLAGS) $(OBJ) -o $@
	
clean:
	$(RM) $(EXEC) $(OBJ)

-include $(DEP)