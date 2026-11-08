return function(c)
    return Package {
        name = "processd",
        version = "0.0.1",
        revision = 1,
        source = c.source("userspace/processd"),
        dependencies = c.deps {
            c.pkg("mlibc"),
            c.pkg("mlibc-headers"),
            c.pkg("pmoscxx"),
            c.pkg("frigg"),
            c.cxx(),
            c.toolchain(),
        },
        runtime_dependencies = c.deps {
            c.pkg("mlibc"),
            c.pkg("mlibc-headers"),
            c.pkg("pmoscxx"),
            c.pkg("frigg"),
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
cp ${SOURCE_DIR}/processd.yaml "${INSTALL_DIR}/boot/processd.yaml"
        ]=]),
    }
end
