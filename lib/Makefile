ROOTDIR = $(realpath .)/..
OBJDIR = $(ROOTDIR)/sysroot/usr/lib
DEPDIR := .deps
SRCDIR := src

CC = $(ROOTDIR)/bin/cross/bin/x86_64-pmos-gcc
CPP = $(ROOTDIR)/bin/cross/bin/x86_64-pmos-gcc
CXX = $(ROOTDIR)/bin/cross/bin/x86_64-pmos-g++

CFLAGS = -std=c17 
CPPFLAGS = $(DEPFLAGS)
LDFLAGS = 

DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

CFILES := $(shell find ./src/ -type f -name '*.c')
ASMFILES := $(shell find ./src/ -type f -name '*.S')
OBJ    := $(ASMFILES:.S=.o) $(CFILES:.c=.o) 
OBJ_P := $(patsubst ./src%,$(OBJDIR)% , $(OBJ))

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(CC)
	@mkdir -p $(DEPDIR)
	@mkdir -p $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.S $(CC)
	@mkdir -p $(DEPDIR)
	@mkdir -p $(OBJDIR)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -c -o $@ $<


libc: $(OBJ_P)

$(DEPDIR):
	@mkdir -p $@

$(OBJDIR):
	@mkdir -p $@	

clean:
	rm -rf $(OBJ_P) $(DEPDIR)

-include $(OBJ: src/%.o, .deps/%.d)

$(CC):
	$(MAKE) -C ../src x86_64-pmos-gcc