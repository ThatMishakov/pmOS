SYSROOT = $(HOME)/pmos/sysroot/

RUSTFLAGS=-C link-arg=--sysroot=$(SYSROOT) -C linker=clang++ -C link-arg=--target=x86_64-unknown-pmos -C link-arg=-lunwind -C link-arg=-static
PROGDIR = $(realpath .)

# TODO: i686 and risc-v memes

export RUSTFLAGS
bin/pmbus:
	cargo +dev-x86_64-unknown-pmos install --target x86_64-unknown-pmos --root . --path . --force

.PHONY: bin/pmbus