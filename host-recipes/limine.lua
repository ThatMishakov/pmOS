return function(c)
    -- Build one host bundle containing all supported bootloader targets.
    return Tool {
        name = "limine",
        version = "10.8.5",
        revision = 1,
        source = c.source("limine"),
        dependencies = { "clang", "lld", "llvm", "nasm", "build-essential", "mtools", "pkg-config" },
        configure = [=[
X86_FLAGS="--enable-bios \
               --enable-bios-cd \
               --enable-bios-pxe \
               --enable-uefi-ia32 \
    "

    # CFLAGS="$HOST_CFLAGS" \
    # CXXFLAGS="$HOST_CXXFLAGS" \
    # LDFLAGS="$HOST_LDFLAGS -static" \
"${SOURCE_DIR}"/configure \
    --prefix="${PREFIX}" \
    --enable-uefi-cd \
    --enable-uefi-x86-64 \
    --enable-uefi-aarch64 \
    --enable-uefi-loongarch64 \
    --enable-uefi-riscv64 \
    $X86_FLAGS
        ]=],
        build = [=[
make -j${PARALLELISM}
        ]=],
        install = [=[
DESTDIR="${INSTALL_DIR}" make install


        ]=],
    }
end
