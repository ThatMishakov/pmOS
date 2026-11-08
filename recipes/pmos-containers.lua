return function(c)
    return Package {
        name = "pmos-containers",
        version = "0.0.1",
        revision = 1,
        source = c.source("libs/pmos-containers/"),
        dependencies = c.deps { "make" },
        install = c.script([=[
DESTDIR="${INSTALL_DIR}" make -C ${SOURCE_DIR} install
        ]=]),
    }
end
