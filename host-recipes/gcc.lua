return function(c)
    return Tool {
        name = "gcc",
        version = "16",
        revision = 1,
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
            c.pkg("mlibc-headers"),
            c.tool("binutils"),
        },
        configure = c.script([=[
# Each build directory supplies libc for one architecture.
${SOURCE_DIR}/configure --target=${PMOS_ARCH}-pmos --with-sysroot="${SYSROOT_DIR}" --with-gxx-include-dir="${SYSROOT_DIR}/usr/include/c++" --enable-languages=c,c++ --disable-multilib
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM} all-gcc all-target-libgcc
        ]=]),
        install = c.script([=[
make DESTDIR="${INSTALL_DIR}" install-gcc install-target-libgcc
        ]=]),
    }
end
