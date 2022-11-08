CC = x86_64-elf-gcc
CPP = x86_64-elf-g++

LIBEXEC = libpm.a

CPPFLAGS = -MMD -MP
LDFLAGS = 

CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJ    := $(ASMFILES:.S=.o) $(CFILES:.c=.o) 

$(LIBEXEC): $(OBJ)
	ar -rv $@ $(OBJ)

clean:
	rm -rf $(OBJ) $(LIBEXEC)