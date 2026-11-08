return function(c)
    return Package {
        name = "pmos-rust-lib",
        version = "0.0.1",
        revision = 1,
        source = c.source("libs/rust"),
        dependencies = c.deps { c.tool("rust") },
        install = c.script([=[
mkdir -p "${INSTALL_DIR}${PREFIX}/src/rust/pmos-rust-lib"
cp -r "${SOURCE_DIR}" "${INSTALL_DIR}${PREFIX}/src/rust/pmos-rust-lib/rust"
        ]=]),
    }
end
