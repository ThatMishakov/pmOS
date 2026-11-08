return function(c)
    return Package {
        name = "libsmarter",
        version = "810746abbf03db3d95e49754a137d4417e216832",
        revision = 1,
        source = c.source("libsmarter"),
        dependencies = c.deps { "meson", "ninja-build", "pkg-config" },
        configure = c.script([=[
meson_configure \
    --includedir=share/libsmarter/include
        ]=]),
        build = c.script([=[
ninja -j${PARALLELISM}
        ]=]),
        install = c.script([=[
DESTDIR="${INSTALL_DIR}" ninja install

        ]=]),
    }
end
