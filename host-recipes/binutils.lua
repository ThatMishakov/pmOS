return function(c)
    return Tool {
        name = "binutils",
        version = "2.47",
        revision = 1,
        source = c.source("binutils"),
        dependencies = c.deps {
            "gcc",
            "build-essential",
            "pkg-config",
            "libreadline-dev",
            "texinfo",
            "file",
            "libgmp-dev",
            "bison",
            "flex",
            "m4",
            "libmpfr-dev",
        },
        configure = c.script([=[
${SOURCE_DIR}/configure --target=${PMOS_ARCH}-pmos --with-sysroot=${SYSROOT_DIR} --disable-werror
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM}
        ]=]),
        install = c.script([=[
make install DESTDIR="${INSTALL_DIR}"
        ]=]),
    }
end
