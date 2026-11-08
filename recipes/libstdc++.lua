return function(c)
    return Package {
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
        runtime_dependencies = c.deps { c.pkg("mlibc-headers"), c.pkg("mlibc") },
        configure = c.script([=[
# Each build directory supplies libc for one architecture.
${SOURCE_DIR}/configure --target=${PMOS_ARCH}-pmos --with-sysroot="${SYSROOT_DIR}" --prefix=${PREFIX} --with-toolexeclibdir=/usr/lib --with-gxx-include-dir=/usr/include/c++ --enable-languages=c,c++ --enable-shared --with-pic --disable-multilib
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM} all-target-libstdc++-v3 all-target-libatomic
        ]=]),
        install = c.script([=[
make DESTDIR="${INSTALL_DIR}" gxx_include_dir="${PREFIX}/include/c++" \
    install-target-libstdc++-v3 install-target-libatomic
        ]=]),
    }
end
