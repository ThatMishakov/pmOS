return function(c)
    return Package {
        name = "mlibc",
        version = "0.0.1",
        revision = 1,
        source = c.source("mlibc"),
        dependencies = c.deps {
            "build-essential",
            "meson",
            "ninja-build",
            "pkg-config",
            c.pkg("mlibc-headers"),
            c.pkg("frigg"),
            c.pkg("libsmarter"),
            c.toolchain(),
        },
        runtime_dependencies = c.deps { c.pkg("mlibc-headers") },
        configure = c.script([=[
CFLAGS="$TARGET_CFLAGS" \
    CXXFLAGS="$TARGET_CXXFLAGS" \
    LDFLAGS="$TARGET_LDFLAGS" \
meson_configure_noflags \
    -Dno_headers=true \
    -Ddefault_library=both
        ]=]),
        build = c.script([=[
ninja -j${PARALLELISM}
        ]=]),
        install = c.script([=[
DESTDIR="${INSTALL_DIR}" ninja install
        ]=]),
    }
end
