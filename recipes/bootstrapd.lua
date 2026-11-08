return function(c)
    return Package {
        name = "bootstrapd",
        version = "0.0.1",
        revision = 1,
        source = c.source("userspace/bootstrapd"),
        dependencies = c.deps {
            c.source("libyaml"),
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
cmake_configure -DFETCHCONTENT_SOURCE_DIR_LIBYAML="${SOURCES_DIR}/libyaml"
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM}
        ]=]),
        install = c.script([=[
DESTDIR="${INSTALL_DIR}" make install
        ]=]),
    }
end
