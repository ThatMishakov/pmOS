return function(c)
    -- Target libraries belong beside the shared host compiler in lib/rustlib.
    return Tool {
        name = "rust-std",
        version = "1.96.0",
        revision = 1,
        source = c.source("rust"),
        dependencies = c.deps {
            c.tool("rust"),
            "cargo",
            "llvm-dev",
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
rust_target_flags=
if [ -n "$COMPILER_TARGET_FLAGS" ]; then
    rust_target_flags=", \"-C\", \"link-arg=${COMPILER_TARGET_FLAGS}\""
fi
cat <<EOF > bootstrap.toml
change-id = 148671

[llvm]
download-ci-llvm = false
link-shared = true

[build]
rustc = "${PREFIX}/bin/rustc"
cargo = "/usr/bin/cargo"
local-rebuild = true
submodules = false
target = ["${RUST_TARGET}"]
build-dir = "${BUILD_DIR}"
docs = false

[target.${RUST_HOST_TARGET}]
llvm-config = "/usr/bin/llvm-config"

[target.${RUST_TARGET}]
linker = "${CXX_COMPILER}"
rustflags = ["-C", "link-arg=--sysroot=${SYSROOT_DIR}", "-C", "link-arg=-l${UNWIND_LIB}"${rust_target_flags}]
EOF
        ]=]),
        build = c.script([=[
cd "${SOURCE_DIR}"
CARGO_HOME="${BUILD_DIR}/.cargo" ./x.py dist --config "${BUILD_DIR}/bootstrap.toml" --stage 0 rust-std --target "${RUST_TARGET}" -j${PARALLELISM}
        ]=]),
        install = c.script([=[
mkdir -p "${INSTALL_DIR}${PREFIX}/lib/rustlib"
cp -rp "${BUILD_DIR}/tmp/tarball/rust-std/${RUST_TARGET}/image/lib/rustlib/${RUST_TARGET}" \
    "${INSTALL_DIR}${PREFIX}/lib/rustlib/"
        ]=]),
    }
end
