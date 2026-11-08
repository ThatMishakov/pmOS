return function(c)
    -- This tool deliberately has no target settings, target packages or build
    -- support sources. Its content hash must be identical for every profile.
    return Tool {
        name = "clang",
        version = "23.1.2",
        revision = 1,
        source = c.source("clang"),
        dependencies = {
            "ninja-build", "cmake", "gcc", "lld", "python3", "build-essential",
            "libedit-dev", "libzstd-dev", "zlib1g-dev",
        },
        configure = [[
cmake -S "${SOURCE_DIR}/llvm" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang;lld;lldb" \
    -DLLVM_TARGETS_TO_BUILD="X86;RISCV;LoongArch" \
    -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=M68k \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DCLANG_DEFAULT_CXX_STDLIB=libc++ \
    -DCLANG_DEFAULT_LINKER=lld
        ]],
        build = [[
ninja -j "${PARALLELISM}"
        ]],
        install = [[
DESTDIR="${INSTALL_DIR}" ninja install
        ]],
    }
end
