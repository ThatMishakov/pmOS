# Shared cross-compilation settings, embedded in each target-specific build step.
LIBRARIES="/usr/lib/ld.so /usr/lib/libc.so /usr/lib/libdl.so /usr/lib/libpthread.so /usr/lib/libpmoscxx.so /usr/lib/libm.so /usr/lib/librt.so /usr/lib/libpmos-helpers.so"

# Selected by Chariot with -o toolchain=clang (the default) or gcc.
: "${PMOS_TOOLCHAIN:=clang}"
case "$PMOS_TOOLCHAIN" in
    gcc)
        C_COMPILER="$PMOS_ARCH-pmos-gcc"
        CXX_COMPILER="$PMOS_ARCH-pmos-g++"
        TOOLS_PREFIX="$PMOS_ARCH-pmos"
        COMPILER_TARGET_FLAGS=""
        MESON_TARGET_ARGS=""
        UNWIND_LIB="gcc_s"
        LIBRARIES="$LIBRARIES /usr/lib/libstdc++.so.6 /usr/lib/libgcc_s.so.1"
        ;;
    clang)
        C_COMPILER="clang"
        CXX_COMPILER="clang++"
        TOOLS_PREFIX="llvm"
        COMPILER_TARGET_FLAGS="--target=${PMOS_ARCH}-unknown-pmos"
        MESON_TARGET_ARGS="'${COMPILER_TARGET_FLAGS}', "
        UNWIND_LIB="unwind"
        LIBRARIES="$LIBRARIES /usr/lib/libc++.so.1 /usr/lib/libc++abi.so.1 /usr/lib/libunwind.so.1"
        ;;
    *) echo "Unknown PMOS_TOOLCHAIN: $PMOS_TOOLCHAIN" >&2; exit 1 ;;
esac

CPU_FAMILY="${PMOS_ARCH}"
case "$PMOS_ARCH" in
    i?86) CPU_FAMILY="x86" ;;
esac

ENDIAN="little"
case "$PMOS_ARCH" in
    m68k) ENDIAN="big" ;;
esac

RUST_HOST_TARGET="$(uname -m)-unknown-linux-gnu"
case "$(uname -m)" in
    i?86) RUST_HOST_TARGET="i686-unknown-linux-gnu" ;;
    arm*) RUST_HOST_TARGET="armv7-unknown-linux-gnueabihf" ;;
    riscv64) RUST_HOST_TARGET="riscv64gc-unknown-linux-gnu" ;;
    ppc64le) RUST_HOST_TARGET="powerpc64le-unknown-linux-gnu" ;;
esac

RUST_TARGET="${PMOS_ARCH}-unknown-pmos"
case "$PMOS_ARCH" in
    x86_64) RUST_TARGET="x86_64-unknown-pmos" ;;
    i686) RUST_TARGET="i686-unknown-pmos" ;;
    riscv64) RUST_TARGET="riscv64gc-unknown-pmos" ;;
    loongarch64) RUST_TARGET="loongarch64-unknown-pmos" ;;
esac

TARGET_CFLAGS=""
TARGET_CXXFLAGS=""
TARGET_LDFLAGS=""
COMPILE_FLAGS=""

meson_configure() {
        CFLAGS="$TARGET_CFLAGS" \
        CXXFLAGS="$TARGET_CXXFLAGS" \
        LDFLAGS="$TARGET_LDFLAGS" \
    meson_configure_noflags "$@"
}

meson_configure_noflags() {
    if [ -z "${meson_source_dir-}" ]; then
        meson_source_dir="${SOURCE_DIR}"
    fi

        PMOS_ARCH=${PMOS_ARCH} \
        SYSROOT_DIR=${SYSROOT_DIR} \
        CPU_FAMILY=${CPU_FAMILY} \
        ENDIAN=${ENDIAN} \
        MESON_TARGET_ARGS="${MESON_TARGET_ARGS}" \
        C_COMPILER=${C_COMPILER} \
        CXX_COMPILER=${CXX_COMPILER} \
        TOOLS_PREFIX=${TOOLS_PREFIX} \
    envsubst > ./cross-file.txt <<'PMOS_MESON_CROSS_FILE'
@MESON_CROSS_FILE@
PMOS_MESON_CROSS_FILE

    meson setup "${meson_source_dir}" \
        --cross-file=cross-file.txt \
        --prefix=${PREFIX} \
        --sysconfdir=/etc \
        --localstatedir=/var \
        --libdir=lib \
        --sbindir=sbin \
        --bindir=bin \
        --datadir=share \
        --buildtype=release \
        -Ddefault_library=shared \
        "$@"
}

cmake_configure() {
    if [ -z "${cmake_source_dir-}" ]; then
        cmake_source_dir="${SOURCE_DIR}"
    fi

        PMOS_ARCH=${PMOS_ARCH} \
        C_COMPILER=${C_COMPILER} \
        CXX_COMPILER=${CXX_COMPILER} \
        SYSROOT_DIR=${SYSROOT_DIR} \
    envsubst > ./cross-file.txt <<'PMOS_CMAKE_TOOLCHAIN'
@CMAKE_TOOLCHAIN@
PMOS_CMAKE_TOOLCHAIN

    cmake "${cmake_source_dir}"  \
        -DTARGET_ARCH=${PMOS_ARCH} \
        -DCMAKE_TOOLCHAIN_FILE="$(pwd -P)/cross-file.txt" \
        -DCMAKE_INSTALL_PREFIX=${PREFIX} \
        -DCMAKE_INSTALL_SYSCONFDIR=/etc \
        -DCMAKE_INSTALL_LOCALSTATEDIR=/var \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_INSTALL_SBINDIR=bin \
        "$@"
}
