return function(c)
    return Package {
        name = "hyper-config",
        version = "1",
        revision = 1,
        source = c.source("scripts"),
        env = { PMOS_KERNEL_ARCH = c.kernel_arch },
        dependencies = c.deps {  },
        build = c.script([=[
FILE_NAME=hyper.cfg
LIBRARIES=$LIBRARIES ${SOURCE_DIR}/make_hyper_config.sh "$FILE_NAME" "$BOOT_SERVICES" "$PMOS_KERNEL_ARCH"
        ]=]),
        install = c.script([=[
FILE_NAME=hyper.cfg
mkdir -p "${INSTALL_DIR}/boot"
cp "${FILE_NAME}" "${INSTALL_DIR}/boot/${FILE_NAME}"
        ]=]),
    }
end
