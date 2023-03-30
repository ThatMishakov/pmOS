Building GCC

export SYSROOT=~/pmos/sysroot
export PREFIX=~/cross
 
../binutils-gdb/configure --target=x86_64-pmos --prefix="$(PREFIX)" --with-sysroot=$(SYSROOT) --disable-werror
../gcc/configure --target=x86_64-pmos --prefix="$(PREFIX)" --with-sysroot=$(SYSROOT) --enable-languages=c,c++
