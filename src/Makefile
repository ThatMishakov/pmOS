ROOTDIR = $(realpath .)/..
PREFIX := $(ROOTDIR)/bin/cross
SYSROOT := $(ROOTDIR)/sysroot

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
	../gcc/configure --target=x86_64-pmos --prefix="$(PREFIX)" --with-sysroot=$(SYSROOT) --enable-languages=c,c++,ada

x86_64-pmos-gcc: $(ROOTDIR)/bin/cross/bin/x86_64-pmos-gcc
$(ROOTDIR)/bin/cross/bin/x86_64-pmos-gcc: build-gcc-pmos
	@mkdir -p $(ROOTDIR)/bin/cross/
	$(MAKE) -C build-gcc-pmos all-gcc all-target-libgcc
	$(MAKE) -C build-gcc-pmos install-gcc install-target-libgcc

x86_64-pmos-ld: $(ROOTDIR)/bin/cross/bin/x86_64-pmos-ld
$(ROOTDIR)/bin/cross/bin/x86_64-pmos-ld: build-binutils-pmos
	@mkdir -p $(ROOTDIR)/bin/cross/
	$(MAKE) -C build-binutils-pmos
	$(MAKE) -C build-binutils-pmos install

distclean:
	rm -rf build-binutils-pmos build-gcc-pmos binutils-gdb gcc



