SYSROOT = $(HOME)/pmos/sysroot/

ARCH ?= x86_64

RUSTFLAGS=-C link-arg=--sysroot=$(SYSROOT) -C linker=clang++ -C link-arg=--target=$(ARCH)-unknown-pmos -C link-arg=-lunwind -C link-arg=-static
PROGDIR = $(realpath .)

# TODO: i686 and risc-v memes

export RUSTFLAGS
bin/pmbus:
	cargo +dev-x86_64-unknown-pmos install --target $(ARCH)-unknown-pmos --root . --path . --force

.PHONY: bin/pmbus