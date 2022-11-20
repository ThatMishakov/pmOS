CC = x86_64-elf-gcc
CPP = x86_64-elf-g++


ROOTDIR = $(realpath .)/..
LIBEXEC = libpm.a
INCLUDE = $(ROOTDIR)/kernel/include $(ROOTDIR)/lib/include
INCPARAM = $(foreach d, $(INCLUDE), -I$d)

CFLAGS = -std=c17 
CPPFLAGS = -MMD -MP $(INCPARAM)
LDFLAGS = 

CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJ    := $(ASMFILES:.S=.o) $(CFILES:.c=.o) 

$(LIBEXEC): $(OBJ)
	ar -rv $@ $(OBJ)

clean:
	rm -rf $(OBJ) $(LIBEXEC)