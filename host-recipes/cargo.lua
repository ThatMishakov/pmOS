return function(c)
    return Tool {
        name = "cargo",
        version = "0.95.0",
        revision = 1,
        source = c.source("cargo"),
        dependencies = c.deps { "build-essential", "gcc", "libssl-dev", "pkg-config", "cargo", c.tool("rust") },
        configure = c.script([=[
rm -rf cargo-source
cp -rp "${SOURCE_DIR}"/. cargo-source
cd cargo-source
CARGO_HOME="${BUILD_DIR}/cargo-home" cargo fetch --locked --target "${RUST_HOST_TARGET}"
        ]=]),
        build = c.script([=[
cd cargo-source
CARGO_HOME="${BUILD_DIR}/cargo-home" CARGO_TARGET_DIR="${BUILD_DIR}/target" \
cargo build --locked --release --offline --target "${RUST_HOST_TARGET}" -j"${PARALLELISM}"
        ]=]),
        install = c.script([=[
find target/${RUST_HOST_TARGET}/release -maxdepth 1 -type f -executable -exec install -D -m755 -t "${INSTALL_DIR}${PREFIX}/bin" {} +
        ]=]),
    }
end
