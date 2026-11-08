return function(c)
    return Package {
        name = "kernel",
        version = "0.0.1",
        revision = 1,
        source = c.source("kernel"),
        dependencies = c.deps {
            c.pkg("mlibc-headers"),
            c.pkg("mlibc"),
            c.pkg("pmos-containers"),
            c.cxx(),
            c.toolchain(),
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
