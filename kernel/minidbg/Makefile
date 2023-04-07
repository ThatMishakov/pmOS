ARCH ?= x86_64
TARGET = libdbg.a
CC = $(ARCH)-elf-gcc
AR = $(ARCH)-elf-ar

SRCS = $(wildcard *.c) $(wildcard $(ARCH)/*.c) $(wildcard $(ARCH)/*.S)
OBJS1 = $(SRCS:.c=.o)
OBJS2 = $(OBJS1:.S=.o)
OBJS = $(subst $(ARCH)/,./,$(OBJS2))
CFLAGS = -ansi -pedantic -Wall -Wextra -O2 -ffreestanding -fno-stack-protector -nostdinc -nostdlib -I./$(ARCH)

all: $(TARGET)

objs: clean $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: $(ARCH)/%.c
	$(CC) $(CFLAGS) -c $< -o $(subst $(ARCH)/,,$@)

%.o: $(ARCH)/%.S
	$(CC) $(CFLAGS) -c $< -o $(subst $(ARCH)/,,$@)

$(TARGET): $(OBJS)
	@rm $@ 2>/dev/null || true
	$(AR) -frsv $@ $^ >/dev/null
	@rm $(OBJS) || true

clean:
	rm $(TARGET) *.o || true

