return function(c)
    return Package {
        name = "tests",
        version = "0.0.1",
        revision = 1,
        source = c.source("userspace/tests"),
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
cp ${SOURCE_DIR}/tests.yaml "${INSTALL_DIR}/boot/tests.yaml"
mkdir -p "${INSTALL_DIR}/root"
cp ${SOURCE_DIR}/test_file.txt "${INSTALL_DIR}/root/test_file.txt"
        ]=]),
    }
end
