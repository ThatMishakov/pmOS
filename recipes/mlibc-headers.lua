return function(c)
    return Package {
        name = "mlibc-headers",
        version = "0.0.1",
        revision = 1,
        source = c.source("mlibc"),
        dependencies = c.deps { "meson", "ninja-build", "pkg-config", "clang", "lld", "llvm" },
        configure = c.script([=[
C_COMPILER=clang
CXX_COMPILER=clang++
TOOLS_PREFIX=llvm
MESON_TARGET_ARGS="'--target=${PMOS_ARCH}-unknown-pmos', "
meson_configure \
    -Dheaders_only=true
        ]=]),
        build = c.script([=[
ninja -j${PARALLELISM}
        ]=]),
        install = c.script([=[
DESTDIR="${INSTALL_DIR}" ninja install
        ]=]),
    }
end
