return function(c)
    return Package {
        name = "i8042",
        version = "0.0.1",
        revision = 1,
        source = c.source("userspace/i8042"),
        dependencies = c.deps {
            c.pkg("mlibc"),
            c.pkg("mlibc-headers"),
            c.pkg("pmos-helpers"),
            c.cxx(),
            c.toolchain(),
        },
        runtime_dependencies = c.deps {
            c.pkg("mlibc"),
            c.pkg("mlibc-headers"),
            c.pkg("pmos-helpers"),
            c.cxx(),
        },
        configure = c.script([=[
cmake_configure
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM}
        ]=]),
        install = c.script([=[
DESTDIR="${INSTALL_DIR}" make install
cp ${SOURCE_DIR}/i8042.yaml "${INSTALL_DIR}/boot/i8042.yaml"
        ]=]),
    }
end
