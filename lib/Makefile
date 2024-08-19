all: ../sysroot/lib/libc.a
libc: ../sysroot/lib/libc.a
lib: libc

ARCH ?= x86_64

clean:
	rm -rf build
	rm -rf ../sysroot/lib/libc.a
	rm -rf ../sysroot/lib/libm.a

build/CMakeCache.txt: CMakeLists.txt
	mkdir -p build
	cd build && cmake .. -DTARGET_ARCH=$(ARCH)

../sysroot/lib/libc.a: build/CMakeCache.txt 
	make -C build install
