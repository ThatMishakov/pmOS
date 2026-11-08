return function(c)
    return Package {
        name = "logd",
        version = "0.0.1",
        revision = 1,
        source = c.source("userspace/logd"),
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
cp ${SOURCE_DIR}/logd.yaml "${INSTALL_DIR}/boot/logd.yaml"
        ]=]),
    }
end
