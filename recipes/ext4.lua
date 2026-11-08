return function(c)
    return Package {
        name = "ext4",
        version = "1.0.0",
        revision = 1,
        source = c.source("userspace/ext4"),
        dependencies = c.deps {
            c.pkg("mlibc"),
            c.pkg("mlibc-headers"),
            c.pkg("pmos-rust-lib"),
            c.cxx(),
            c.toolchain(),
            "cargo",
            c.tool("rust"),
            c.tool("rust-std"),
        },
        runtime_dependencies = c.deps {
            c.pkg("mlibc"),
            c.pkg("mlibc-headers"),
            c.pkg("pmos-rust-lib"),
            c.cxx(),
        },
        configure = c.script([=[
cp -rp "${SOURCE_DIR}"/. ./
# The crate is built from a writable copy; resolve its packaged pmOS dependency here.
sed -i "s|/sysroot/|${SYSROOT_DIR}/|g" Cargo.toml
        ]=]),
        build = c.script([=[
CARGO_HOME="${BUILD_DIR}/cargo-home" \
RUSTFLAGS="-C link-arg=--sysroot=${SYSROOT_DIR} -C linker=${CXX_COMPILER} ${COMPILER_TARGET_FLAGS:+-C link-arg=${COMPILER_TARGET_FLAGS}} -C link-arg=-l${UNWIND_LIB}" \
TARGET_CC="${C_COMPILER}" \
cargo build --locked --release --target "${RUST_TARGET}" -j"${PARALLELISM}"
        ]=]),
        install = c.script([=[
mkdir -p "${INSTALL_DIR}/boot"
cp "${BUILD_DIR}/target/${RUST_TARGET}/release/ext4" "${INSTALL_DIR}/boot/"
cp "${SOURCE_DIR}/ext4.yaml" "${INSTALL_DIR}/boot/ext4.yaml"
        ]=]),
    }
end
