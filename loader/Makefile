# Executable name
EXEC = loader.elf

# It is highly recommended to use a custom built cross toolchain to build a kernel.
# We are only using "cc" as a placeholder here. It may work by using
# the host system's toolchain, but this is not guaranteed.
CC = clang
CPP = clang
CXX = clang++
AS = nasm
TARGET := i686-elf

INCLUDEDIR = include

# User controllable CFLAGS.
USERCFLAGS = -Wall -Wextra -O2 -pipe -target $(TARGET) 

CFLAGS = -mcmodel=kernel $(INTERNALCFLAGS) -I$(INCLUDEDIR) $(USERCFLAGS) -m32
CPPFLAGS = $(CFLAGS)
ASMFLAGS = -I$(INCLUDEDIR) --32
LDFLAGS = $(INTERNALLDFLAGS) -target $(TARGET)

# Internal link flags that should not be changed by the user.
INTERNALLDFLAGS :=     \
    -nostdlib               \
    -static                 \
	-Tlinker.ld    \
	-z max-page-size=0x1000
 
# Internal C flags that should not be changed by the user.
INTERNALCFLAGS  :=       \
	-I.                  \
	-ffreestanding       \
	-fno-stack-protector \
	-fPIC -fPIE       \
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
	$(CC) $(LDFLAGS) $(OBJ) -o $@

	
clean:
	rm -f $(EXEC) *.o *.s

all: $(EXEC)
