return function(c)
    if c.kernel_arch ~= chariot.target_arch then
        -- Boot data for the ia32 image, built with a separate amd64 sysroot.
        -- Make exports this package before evaluating the compat image profile.
        return Package {
            name = "kernel-x86_64",
            version = "0.0.1",
            revision = 1,
            source = Source {
                name = "kernel-x86_64",
                Local(c.kernel_binary_dir),
            },
            install = [[
install -Dm755 "${SOURCE_DIR}/kernel" "${INSTALL_DIR}/boot/kernel"
            ]],
        }
    end
    return Package {
        name = "kernel",
        version = "0.0.1",
        revision = 1,
        source = c.source("kernel"),
        dependencies = c.deps {
            c.pkg("mlibc-headers"),
            c.pkg("mlibc"),
            c.pkg("pmos-containers"),
            c.cxx(),
            c.toolchain(),
        },
        configure = c.script([=[
cmake_configure
        ]=]),
        build = c.script([=[
make -j ${PARALLELISM}
        ]=]),
        install = c.script([=[
DESTDIR="${INSTALL_DIR}" make install
        ]=]),
    }
end
