return function(c)
    return Tool {
        name = "xgcc",
        version = "16",
        revision = 2,
        source = c.source("gcc"),
        dependencies = c.deps {
            "gcc",
            "build-essential",
            "pkg-config",
            "libgmp-dev",
            "libmpfr-dev",
            "libmpc-dev",
            "flex",
            "texinfo",
            "bison",
            "libisl-dev",
            "gettext",
            c.tool("binutils"),
        },
        configure = c.script([=[
${SOURCE_DIR}/configure --target=${PMOS_ARCH}-pmos --with-sysroot="${SYSROOT_DIR}" \
    --enable-languages=c,c++ \
    --with-newlib \
    --without-headers \
    --disable-shared \
    --disable-threads \
    --disable-nls \
    --disable-multilib \
    --enable-initfini-array \
    --disable-hosted-libstdcxx \
    --disable-fixincludes \
    --disable-wchar_t \
    --disable-libatomic \
    --disable-libgomp \
    --disable-libitm \
    --disable-libssp \
    --disable-libsanitizer \
    --disable-libquadmath \
    --disable-libvtv
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM} \
    all-gcc \
    all-target-libgcc
        ]=]),
        install = c.script([=[
DESTDIR="${INSTALL_DIR}" make \
    install-gcc \
    install-target-libgcc
        ]=]),
    }
end
