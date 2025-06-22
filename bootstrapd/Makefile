all: build/bootstrapd

ARCH ?= x86_64

clean:
	rm -rf build/

build/CMakeCache.txt: CMakeLists.txt
	mkdir -p build
	cd build && cmake .. -DTARGET_ARCH=$(ARCH)

build/bootstrapd: build/CMakeCache.txt
	make -C build

.PHONY: build/bootstrapd