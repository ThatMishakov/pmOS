CC = clang
CPP = clang
TARGET := x86_64-elf

LIBEXEC = libpm.a

CPPFLAGS = -MMD -MP
LDFLAGS = -target $(TARGET)

CFILES := $(shell find ./ -type f -name '*.c')
ASMFILES := $(shell find ./ -type f -name '*.S')
OBJ    := $(ASMFILES:.S=.o) $(CFILES:.c=.o) 

$(LIBEXEC): $(OBJ)
	ar -rv $@ $(OBJ)

clean:
	rm -rf $(OBJ) $(LIBEXEC)