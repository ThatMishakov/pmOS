return function(c)
    return Package {
        name = "pmos-helpers",
        version = "0.1",
        revision = 1,
        source = c.source("libs/pmos-helpers"),
        dependencies = c.deps {
            "make",
            "cmake",
            c.pkg("mlibc-headers"),
            c.pkg("mlibc"),
            c.pkg("pmos-containers"),
            c.cxx(),
            c.toolchain(),
        },
        runtime_dependencies = c.deps {
            c.pkg("mlibc-headers"),
            c.pkg("mlibc"),
            c.pkg("pmos-containers"),
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
        ]=]),
    }
end
