return function(c)
    return Package {
        name = "limine-iso",
        version = "1",
        revision = 1,
        source = c.source("scripts"),
        env = { PATH = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" },
        dependencies = c.deps {
            "xorriso",
            c.pkg("limine-config"),
            c.pkg("kernel"),
            c.pkg("bootstrapd"),
            c.services(),
            c.tool("limine"),
        },
        build = c.script([=[
SYSROOT="$SYSROOT_DIR" \
LIBRARIES=$LIBRARIES \
${SOURCE_DIR}/make_limine_iso.sh "${BUILD_DIR}/pmos.iso" "${BOOT_SERVICES}" "limine.conf"
        ]=]),
        install = c.script([=[
mkdir -p "${INSTALL_DIR}/images"
cp "${BUILD_DIR}/pmos.iso" "${INSTALL_DIR}/images/"
        ]=]),
    }
end
