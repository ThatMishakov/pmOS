SYSROOT = $(HOME)/pmos/sysroot/

ARCH ?= x86_64
RUSTARCH = $(ARCH)

RUSTFLAGS=-C link-arg=--sysroot=$(SYSROOT) -C linker=clang++ -C link-arg=--target=$(ARCH)-unknown-pmos -C link-arg=-lunwind -C link-arg=-static
PROGDIR = $(realpath .)

ifeq ($(ARCH), riscv64)
	RUSTARCH=riscv64gc
	RUSTFLAGS += -C link-arg=-Wl,--unresolved-symbols=ignore-in-object-files
endif

# TODO: i686 and risc-v memes

export RUSTFLAGS
bin/pmbus:
	cargo +dev-x86_64-unknown-pmos install --target $(RUSTARCH)-unknown-pmos --root . --path . --force

.PHONY: bin/pmbus