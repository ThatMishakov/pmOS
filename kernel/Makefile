all: build/kernel

clean:
	rm -rf build/

build/CMakeCache.txt: CMakeLists.txt
	mkdir -p build
	cd build && cmake ..

build/kernel: build/CMakeCache.txt
	make -C build

.PHONY: build/kernel

