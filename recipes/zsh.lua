return function(c)
    return Package {
        name = "zsh",
        version = "5.9.2",
        revision = 2,
        source = c.source("zsh"),
        dependencies = c.deps { "build-essential", c.pkg("ncurses"), c.pkg("mlibc"), c.cxx(), c.toolchain() },
        runtime_dependencies = c.deps { c.pkg("ncurses"), c.pkg("mlibc"), c.cxx() },
        configure = c.script([=[
CC="${C_COMPILER} ${COMPILER_TARGET_FLAGS} --sysroot=${SYSROOT_DIR}" \
CXX="${CXX_COMPILER} ${COMPILER_TARGET_FLAGS} --sysroot=${SYSROOT_DIR}" \
CPP="${C_COMPILER} ${COMPILER_TARGET_FLAGS} --sysroot=${SYSROOT_DIR} -E" \
CPPFLAGS="-D_GNU_SOURCE" \
${SOURCE_DIR}/configure --build=$(uname -m)-pc-linux-gnu --host=$PMOS_ARCH-pmos --prefix=${PREFIX} --bindir=/bin
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM}
        ]=]),
        install = c.script([=[
make install DESTDIR=${INSTALL_DIR}

mkdir -p ${INSTALL_DIR}${PREFIX}/bin
pushd ${INSTALL_DIR}${PREFIX}/bin
ln -sv /bin/zsh zsh
popd
        ]=]),
    }
end
