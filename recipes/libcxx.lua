return function(c)
    return Package {
        name = "libcxx",
        version = "23.1.2",
        revision = 4,
        source = c.source("clang"),
        dependencies = c.deps {
            "python3",
            "build-essential",
            c.pkg("mlibc-headers"),
            c.pkg("mlibc"),
            c.tool("clang"),
            c.tool("compiler-rt"),
        },
        runtime_dependencies = c.deps { c.pkg("mlibc-headers"), c.pkg("mlibc") },
        configure = c.script([=[
triplet="${PMOS_ARCH}-pmos"
EXTRA_FLAGS=
if [ "${PMOS_ARCH}" = i686 ]; then EXTRA_FLAGS=-DCAN_TARGET_i386=1; fi
cmake -S ${SOURCE_DIR}/runtimes \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/ \
    -DLLVM_TARGET_TRIPLE=${triplet} \
    -DCMAKE_CXX_COMPILER_TARGET="${triplet}" \
    -DCMAKE_C_COMPILER="clang" \
    -DCMAKE_CXX_COMPILER="clang++" \
    -DCMAKE_C_COMPILER_TARGET="${triplet}" \
    -DCMAKE_ASM_COMPILER_TARGET="${triplet}" \
    -DCOMPILER_RT_BUILD_BUILTINS=ON \
    -DCMAKE_C_FLAGS="--sysroot=${SYSROOT_DIR} -nostartfiles -lclang_rt.builtins" \
    -DCMAKE_CXX_FLAGS="--sysroot=${SYSROOT_DIR} -nostdlib -lclang_rt.builtins" \
    -DLIBCXXABI_USE_LLVM_UNWINDER=true \
    -DCMAKE_C_COMPILER_WORKS=ON \
    -DCMAKE_CXX_COMPILER_WORKS=ON \
    ${EXTRA_FLAGS}
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM}
        ]=]),
        install = c.script([=[
DESTDIR="${INSTALL_DIR}" make install
        ]=]),
    }
end
