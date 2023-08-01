ROOTDIR = $(realpath .)/..
OBJDIR = $(ROOTDIR)/sysroot/usr/lib
LIBC := $(OBJDIR)/libc.a
LIBM := $(OBJDIR)/libm.a

all: libc libm

libc: $(LIBC)
libm: $(LIBM)

$(LIBC):
	$(MAKE) -C libc $(LIBC)

$(LIBM):
	$(MAKE) -C libm $(LIBM)

.PHONY: $(LIBC) $(LIBM)