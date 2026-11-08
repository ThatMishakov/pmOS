return function(c)
    return Tool {
        name = "libstdc++",
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
            c.pkg("mlibc"),
            c.toolchain(),
        },
        configure = c.script([=[
multilib=
if [ "${PMOS_ARCH}" = "m68k" ]; then
    multilib=--disable-multilib
fi
${SOURCE_DIR}/configure --target=${PMOS_ARCH}-pmos --with-sysroot="${SYSROOT_DIR}" --prefix=${PREFIX} --with-toolexeclibdir=/usr/lib --enable-languages=c,c++ --enable-shared --with-pic ${multilib}
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM} all-target-libstdc++-v3 all-target-libatomic
        ]=]),
        install = c.script([=[
make DESTDIR="${INSTALL_DIR}" install-target-libstdc++-v3 install-target-libatomic
        ]=]),
    }
end
