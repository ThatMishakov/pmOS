ROOTDIR = $(realpath .)/..
PREFIX := $(ROOTDIR)/bin/cross
SYSROOT := $(ROOTDIR)/sysroot
TARGET = x86_64-pmos

export PATH := $(ROOTDIR)/bin/cross/bin:$(PATH)

all: x86_64-pmos-ld x86_64-pmos-gcc

binutils-gdb:
	git clone git://sourceware.org/git/binutils-gdb.git --depth=1 --branch=binutils-2_39
	cd binutils-gdb; \
	git apply ../pmos_binutils.patch

binutils: binutils-gdb

gcc:
	git clone git://gcc.gnu.org/git/gcc.git --depth=1 --branch="releases/gcc-12.2.0"
	cd gcc ; \
	git apply ../pmos_gcc.patch

build-binutils-pmos: binutils-gdb
	mkdir build-binutils-pmos
	cd build-binutils-pmos; \
	../binutils-gdb/configure --target=x86_64-pmos --prefix="$(PREFIX)" --with-sysroot=$(SYSROOT) --disable-werror

build-gcc-pmos: gcc
	mkdir build-gcc-pmos
	cd build-gcc-pmos; \
	../gcc/configure --target=x86_64-pmos --prefix="$(PREFIX)" --with-sysroot=$(SYSROOT) --enable-languages=c,c++

x86_64-pmos-gcc: $(ROOTDIR)/bin/cross/bin/x86_64-pmos-gcc
$(ROOTDIR)/bin/cross/bin/x86_64-pmos-gcc: build-gcc-pmos x86_64-pmos-ld
	@mkdir -p $(ROOTDIR)/bin/cross/
	$(MAKE) -C build-gcc-pmos all-gcc
	$(MAKE) -C build-gcc-pmos all-target-libgcc CFLAGS_FOR_TARGET='-g -O2 -mcmodel=kernel -mno-red-zone' || true
	sed -i 's/PICFLAG/DISABLED_PICFLAG/g' build-gcc-pmos/$(TARGET)/libgcc/Makefile
	$(MAKE) -C build-gcc-pmos all-target-libgcc CFLAGS_FOR_TARGET='-g -O2 -mcmodel=kernel -mno-red-zone'
	$(MAKE) -C build-gcc-pmos install-gcc install-target-libgcc

x86_64-pmos-ld: $(ROOTDIR)/bin/cross/bin/x86_64-pmos-ld
$(ROOTDIR)/bin/cross/bin/x86_64-pmos-ld: build-binutils-pmos
	@mkdir -p $(ROOTDIR)/bin/cross/
	$(MAKE) -C build-binutils-pmos
	$(MAKE) -C build-binutils-pmos install

distclean:
	rm -rf build-binutils-pmos build-gcc-pmos binutils-gdb gcc



