#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CHARIOT=${CHARIOT:-"$PROJECT_DIR/.chariot-tools/bin/chariot"}
cd "$PROJECT_DIR"

# Build the host tool once before running this check. Every other profile must
# resolve to exactly that cached artifact, without building another compiler.
LLVM_STORE=
for arch in x86_64 i686 riscv64 loongarch64 m68k; do
    for toolchain in clang gcc; do
        if ! store=$("$CHARIOT" lookup --arch "$arch" -o "toolchain=$toolchain" \
            --allow-new-profiles --tool clang); then
            echo "Missing shared LLVM artifact for $arch/$toolchain; build --tool clang first." >&2
            exit 1
        fi
        if [ -z "$LLVM_STORE" ]; then
            LLVM_STORE=$store
        elif [ "$store" != "$LLVM_STORE" ]; then
            echo "LLVM cache differs for $arch/$toolchain: $store != $LLVM_STORE" >&2
            exit 1
        fi
    done
done

echo "All architecture/toolchain profiles share LLVM: $LLVM_STORE"
