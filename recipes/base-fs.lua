return function(c)
    return Package {
        name = "base-fs",
        version = "1",
        revision = 1,
        dependencies = c.deps {  },
        install = c.script([=[
mkdir -p ${INSTALL_DIR}/run/initramfs
        ]=]),
    }
end
