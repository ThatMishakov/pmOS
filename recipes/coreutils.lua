return function(c)
    return Package {
        name = "coreutils",
        version = "9.12",
        revision = 1,
        source = c.source("coreutils"),
        dependencies = c.deps { "build-essential", "perl", "texinfo", c.pkg("mlibc"), c.cxx(), c.toolchain() },
        runtime_dependencies = c.deps { c.pkg("mlibc"), c.cxx() },
        configure = c.script([=[
# Coreutils generates some files in its source tree even for an out-of-tree build.
rm -rf coreutils-source
cp -rp "${SOURCE_DIR}"/. coreutils-source
AR="${TOOLS_PREFIX}-ar" \
RANLIB="${TOOLS_PREFIX}-ranlib" \
NM="${TOOLS_PREFIX}-nm" \
STRIP="${TOOLS_PREFIX}-strip" \
CC="${C_COMPILER} ${COMPILER_TARGET_FLAGS} --sysroot=${SYSROOT_DIR}" \
CXX="${CXX_COMPILER} ${COMPILER_TARGET_FLAGS} --sysroot=${SYSROOT_DIR}" \
CPP="${C_COMPILER} ${COMPILER_TARGET_FLAGS} --sysroot=${SYSROOT_DIR} -E" \
"${BUILD_DIR}/coreutils-source/configure" --build=$(uname -m)-pc-linux-gnu --host=$PMOS_ARCH-pmos --prefix=${PREFIX}
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM}
        ]=]),
        install = c.script([=[
make install DESTDIR=${INSTALL_DIR}
        ]=]),
    }
end
