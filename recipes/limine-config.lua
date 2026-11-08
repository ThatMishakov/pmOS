return function(c)
    return Package {
        name = "limine-config",
        version = "1",
        revision = 2,
        source = c.source("scripts"),
        env = { PMOS_KERNEL_ARCH = c.kernel_arch },
        dependencies = c.deps {  },
        build = c.script([=[
CONFIG_FILE="limine.conf"
LIBRARIES=$LIBRARIES \
${SOURCE_DIR}/make_limine_cnf.sh "$CONFIG_FILE" "$BOOT_SERVICES" "$PMOS_KERNEL_ARCH"
        ]=]),
        install = c.script([=[
mkdir -p "${INSTALL_DIR}/boot"
cp "limine.conf" "${INSTALL_DIR}/boot/limine.conf"
        ]=]),
    }
end
