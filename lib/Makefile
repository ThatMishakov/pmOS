ROOTDIR = $(realpath .)/..
OBJDIR = $(ROOTDIR)/sysroot/usr/lib
LIBC := $(OBJDIR)/libc.a

all: libc

libc: $(LIBC)

$(LIBC):
	$(MAKE) -C libc $(LIBC)

.PHONY: $(LIBC)