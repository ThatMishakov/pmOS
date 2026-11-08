return function(c)
    return Package {
        name = "bash",
        version = "5.3",
        revision = 1,
        source = c.source("bash"),
        dependencies = c.deps { "build-essential", c.pkg("readline"), c.pkg("mlibc"), c.cxx(), c.toolchain() },
        runtime_dependencies = c.deps { c.pkg("readline"), c.pkg("mlibc"), c.cxx() },
        configure = c.script([=[
AR="${TOOLS_PREFIX}-ar" \
RANLIB="${TOOLS_PREFIX}-ranlib" \
NM="${TOOLS_PREFIX}-nm" \
STRIP="${TOOLS_PREFIX}-strip" \
CC="${C_COMPILER} ${COMPILER_TARGET_FLAGS} --sysroot=${SYSROOT_DIR}" \
CXX="${CXX_COMPILER} ${COMPILER_TARGET_FLAGS} --sysroot=${SYSROOT_DIR}" \
CPP="${C_COMPILER} ${COMPILER_TARGET_FLAGS} --sysroot=${SYSROOT_DIR} -E" \
CPPFLAGS="-D_GNU_SOURCE" \
CC_FOR_BUILD=gcc \
CFLAGS_FOR_BUILD="-std=gnu17 -g" \
bash_cv_getcwd_malloc=yes \
${SOURCE_DIR}/configure --build=$(uname -m)-pc-linux-gnu --host=$PMOS_ARCH-pmos --prefix=${PREFIX} --without-bash-malloc --with-installed-readline
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM}
        ]=]),
        install = c.script([=[
make install DESTDIR=${INSTALL_DIR}

mkdir -p ${INSTALL_DIR}/bin
pushd ${INSTALL_DIR}/bin
ln -sv ${PREFIX}/bin/bash sh
ln -sv ${PREFIX}/bin/bash bash
popd
        ]=]),
    }
end
