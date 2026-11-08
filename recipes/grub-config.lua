return function(c)
    return Package {
        name = "grub-config",
        version = "1",
        revision = 2,
        source = c.source("scripts"),
        dependencies = c.deps {  },
        build = c.script([=[
CONFIG_FILE="grub.cfg"
LIBRARIES=$LIBRARIES \
${SOURCE_DIR}/make_grub_config.sh "$CONFIG_FILE" "$BOOT_SERVICES"
        ]=]),
        install = c.script([=[
mkdir -p "${INSTALL_DIR}/boot"
cp "grub.cfg" "${INSTALL_DIR}/boot/grub.cfg"
        ]=]),
    }
end
