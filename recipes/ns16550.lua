return function(c)
    return Package {
        name = "ns16550",
        version = "0.0.1",
        revision = 1,
        source = c.source("userspace/ns16550"),
        dependencies = c.deps {
            c.pkg("mlibc"),
            c.pkg("mlibc-headers"),
            c.pkg("pmoscxx"),
            c.cxx(),
            c.toolchain(),
        },
        runtime_dependencies = c.deps {
            c.pkg("mlibc"),
            c.pkg("mlibc-headers"),
            c.pkg("pmoscxx"),
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
cp ${SOURCE_DIR}/ns16550.yaml "${INSTALL_DIR}/boot/ns16550.yaml"
        ]=]),
    }
end
