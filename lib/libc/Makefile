ROOTDIR = $(realpath .)/../..
OBJDIR = $(ROOTDIR)/sysroot/usr/lib
SRCDIR := src
LIBC := $(OBJDIR)/libc.a

CC = x86_64-pmos-gcc
CPP = x86_64-pmos-gcc
CXX = x86_64-pmos-g++

CFLAGS = -std=c17
CPPFLAGS = $(DEPFLAGS)
LDFLAGS = 

DEPFLAGS = -MT $@ -MMD -MP -g -O1

CFILES := $(shell find ./src/ -type f -name '*.c')
ASMFILES := $(shell find ./src/ -type f -name '*.S')
OBJ    := $(ASMFILES:.S=.o) $(CFILES:.c=.o) 

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(CC)
	@mkdir -p $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.S $(CC)
	@mkdir -p $(OBJDIR)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -c -o $@ $<

libc: $(OBJDIR)/libc.a
$(OBJDIR)/libc.a: $(OBJ)
	ar r $@ $?

$(OBJDIR):
	@mkdir -p $@	

clean:
	rm -rf $(OBJ) $(DEP)

-include $(DEP)

DEP    := $(OBJ:.o=.d)

$(CC):
	$(MAKE) -C ../src x86_64-pmos-gcc