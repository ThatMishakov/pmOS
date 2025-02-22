SYSROOT = $(HOME)/pmos/sysroot/

RUSTFLAGS=-C link-arg=--sysroot=$(SYSROOT) -C linker=clang++ -C link-arg=--target=x86_64-unknown-pmos -C link-arg=-lunwind -C link-arg=-static

# TODO: i686 and risc-v memes

export RUSTFLAGS
target/x86_64-unknown-pmos/debug/pmbus:
	cargo +dev-x86_64-unknown-pmos build --target x86_64-unknown-pmos

.PHONY: target/x86_64-unknown-pmos/debug/pmbus