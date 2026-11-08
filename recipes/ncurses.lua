return function(c)
    return Package {
        name = "ncurses",
        version = "6.5",
        revision = 1,
        source = c.source("ncurses"),
        dependencies = c.deps { "build-essential", c.pkg("mlibc"), c.cxx(), c.toolchain() },
        runtime_dependencies = c.deps { c.pkg("mlibc"), c.cxx() },
        configure = c.script([=[
AR="${TOOLS_PREFIX}-ar" \
RANLIB="${TOOLS_PREFIX}-ranlib" \
NM="${TOOLS_PREFIX}-nm" \
STRIP="${TOOLS_PREFIX}-strip" \
CC="${C_COMPILER} ${COMPILER_TARGET_FLAGS} --sysroot=${SYSROOT_DIR}" \
CXX="${CXX_COMPILER} ${COMPILER_TARGET_FLAGS} --sysroot=${SYSROOT_DIR}" \
CPP="${C_COMPILER} ${COMPILER_TARGET_FLAGS} --sysroot=${SYSROOT_DIR} -E" \
CPPFLAGS="-D_GNU_SOURCE" \
${SOURCE_DIR}/configure --build=$(uname -m)-pc-linux-gnu --host=$PMOS_ARCH-pmos --prefix=${PREFIX} --with-shared --without-normal --with-termlib --enable-pc-files --enable-widec \
	--with-build-cc=gcc \
	--with-build-cpp='gcc -E'
        ]=]),
        build = c.script([=[
make -j${PARALLELISM}
        ]=]),
        install = c.script([=[
# ncurses hardcodes a bare `install -s` (INSTALL_OPT_S), which would strip the
# x86_64 target binaries with the host's native strip and fail on non-x86_64
# hosts. Point install at the cross strip so the binaries are still stripped.
make install DESTDIR="${INSTALL_DIR}" INSTALL_PROG="\$(INSTALL) -s --strip-program=${TOOLS_PREFIX}-strip"

echo "INPUT(-lncursesw)" > ${INSTALL_DIR}${PREFIX}/lib/libncurses.so
echo "INPUT(-ltinfow)" > ${INSTALL_DIR}${PREFIX}/lib/libtinfo.so
echo "INPUT(-lmenuw)" > ${INSTALL_DIR}${PREFIX}/lib/libmenu.so
echo "INPUT(-lformw)" > ${INSTALL_DIR}${PREFIX}/lib/libform.so
echo "INPUT(-lpanelw)" > ${INSTALL_DIR}${PREFIX}/lib/libpanel.so
echo "INPUT(-lncursesw)" > ${INSTALL_DIR}${PREFIX}/lib/libcursesw.so
cp ${INSTALL_DIR}${PREFIX}/lib/libncurses.so ${INSTALL_DIR}${PREFIX}/lib/libcurses.so
        ]=]),
    }
end
