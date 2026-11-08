return function(c)
    return Package {
        name = "limine-disk-image",
        version = "1",
        revision = 4,
        source = c.source("scripts"),
        env = { PATH = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" },
        dependencies = c.deps {
            "e2fsprogs",
            "mtools",
            "fdisk",
            c.pkg("limine-config"),
            c.pkg("kernel"),
            c.pkg("bootstrapd"),
            c.services(),
            c.pkg("bash"),
            c.pkg("ncurses"),
            c.pkg("coreutils"),
            c.pkg("base-fs"),
            c.tool("limine"),
        },
        build = c.script([=[
SYSROOT="$SYSROOT_DIR" \
LIBRARIES=$LIBRARIES \
${SOURCE_DIR}/make_limine_disk_image.sh "${BUILD_DIR}/pmos.img" "${BOOT_SERVICES}" "limine.conf"
        ]=]),
        install = c.script([=[
mkdir -p "${INSTALL_DIR}/images"
cp "${BUILD_DIR}/pmos.img" "${INSTALL_DIR}/images/"
        ]=]),
    }
end
