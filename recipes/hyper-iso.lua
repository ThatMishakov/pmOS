return function(c)
    return Package {
        name = "hyper-iso",
        version = "1",
        revision = 1,
        source = c.source("scripts"),
        env = { PATH = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" },
        dependencies = c.deps {
            "xorriso",
            "e2fsprogs",
            "mtools",
            c.pkg("hyper-config"),
            c.pkg("kernel"),
            c.pkg("bootstrapd"),
            c.services(),
            c.tool("hyper"),
        },
        build = c.script([=[
SYSROOT="$SYSROOT_DIR" \
LIBRARIES=$LIBRARIES \
${SOURCE_DIR}/make_hyper_iso.sh "${BUILD_DIR}/pmos-hyper.iso" "${BOOT_SERVICES}" "hyper.cfg"
        ]=]),
        install = c.script([=[
mkdir -p "${INSTALL_DIR}/images"
cp "${BUILD_DIR}/pmos-hyper.iso" "${INSTALL_DIR}/images/"
        ]=]),
    }
end
