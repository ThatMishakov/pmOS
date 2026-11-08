return function(c)
    return Tool {
        name = "rust",
        version = "1.0.0",
        revision = 1,
        source = c.source("rust"),
        dependencies = c.deps {
            c.pkg("mlibc"),
            c.cxx(),
            c.toolchain(),
            "python3",
            "curl",
            "git",
            "cmake",
            "ninja-build",
            "pkg-config",
            "libssl-dev",
            "zlib1g-dev",
        },
        configure = c.script([=[
# Refresh the writable checkout, retaining compiler outputs outside it.
rm -rf rust-source
cp -rp "${SOURCE_DIR}"/. rust-source
rust_target_flags=
if [ -n "$COMPILER_TARGET_FLAGS" ]; then
    rust_target_flags=", \"-C\", \"link-arg=${COMPILER_TARGET_FLAGS}\""
fi
cat <<EOF > bootstrap.toml
change-id = 148671

[llvm]
download-ci-llvm = false
targets = "X86;RISCV;LoongArch"
experimental-targets = "M68k"

[build]
target = ["${RUST_TARGET}", "${RUST_HOST_TARGET}"]
build-dir = "${BUILD_DIR}"
docs = false

[target.${RUST_TARGET}]
linker = "${CXX_COMPILER}"
rustflags = ["-C", "link-arg=--sysroot=${SYSROOT_DIR}", "-C", "link-arg=-l${UNWIND_LIB}"${rust_target_flags}]

[install]
prefix = "${PREFIX}"
sysconfdir = "${PREFIX}/etc"
EOF
        ]=]),
        build = c.script([=[
CARGO_HOME="${BUILD_DIR}/.cargo" ${BUILD_DIR}/rust-source/x.py build --config "${BUILD_DIR}/bootstrap.toml" --stage 2 -j${PARALLELISM}
        ]=]),
        install = c.script([=[
CARGO_HOME="${BUILD_DIR}/.cargo" DESTDIR="${INSTALL_DIR}" ${BUILD_DIR}/rust-source/x.py install --config "${BUILD_DIR}/bootstrap.toml" --stage 2 -j${PARALLELISM};
    find "${INSTALL_DIR}" -name "*.old" -delete
        ]=]),
    }
end
