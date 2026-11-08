return function(c)
    return Package {
        name = "ps2d",
        version = "0.0.1",
        revision = 1,
        source = c.source("userspace/ps2d"),
        dependencies = c.deps {
            c.pkg("mlibc"),
            c.pkg("mlibc-headers"),
            c.pkg("pmos-helpers"),
            c.toolchain(),
            c.cxx(),
        },
        runtime_dependencies = c.deps { c.pkg("mlibc"), c.pkg("mlibc-headers"), c.pkg("pmos-helpers") },
        configure = c.script([=[
cmake_configure
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM}
        ]=]),
        install = c.script([=[
DESTDIR="${INSTALL_DIR}" make install
cp ${SOURCE_DIR}/ps2d.yaml "${INSTALL_DIR}/boot/ps2d.yaml"
        ]=]),
    }
end
