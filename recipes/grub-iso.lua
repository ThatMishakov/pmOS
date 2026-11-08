return function(c)
    return Package {
        name = "grub-iso",
        version = "1",
        revision = 2,
        source = c.source("scripts"),
        env = { PATH = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" },
        dependencies = c.deps {
            "grub2",
            "xorriso",
            c.pkg("grub-config"),
            c.pkg("kernel"),
            c.pkg("bootstrapd"),
            c.services(),
        },
        build = c.script([=[
SYSROOT="$SYSROOT_DIR" \
LIBRARIES=$LIBRARIES \
${SOURCE_DIR}/make_grub_iso.sh "${BUILD_DIR}/pmos-grub.iso" "${BOOT_SERVICES}" "grub.cfg"
        ]=]),
        install = c.script([=[
mkdir -p "${INSTALL_DIR}/images"
cp "${BUILD_DIR}/pmos-grub.iso" "${INSTALL_DIR}/images/"
        ]=]),
    }
end
