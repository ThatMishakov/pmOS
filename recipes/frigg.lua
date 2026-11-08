return function(c)
    return Package {
        name = "frigg",
        version = "b9ce1199489f25add06d66c5cc355e2c17dbe027",
        revision = 1,
        source = c.source("frigg"),
        dependencies = c.deps { "meson", "ninja-build", "pkg-config" },
        configure = c.script([=[
meson_configure \
    --includedir=include \
    -Dbuild_tests=disabled
        ]=]),
        build = c.script([=[
ninja -j${PARALLELISM}
        ]=]),
        install = c.script([=[
DESTDIR="${INSTALL_DIR}" ninja install

        ]=]),
    }
end
