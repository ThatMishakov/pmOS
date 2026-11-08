return function(c)
    -- Build one host bundle containing all supported bootloader targets.
    return Tool {
        name = "hyper",
        version = "0.10.0",
        revision = 1,
        source = c.source("hyper"),
        dependencies = { "clang", "lld", "nasm", "cmake", "python3" },
        configure = [=[
cp -rp "${SOURCE_DIR}"/. ./
        ]=],
        build = [=[
./build.py
./build.py --arch amd64
./build.py --arch aarch64
        ]=],
        install = [=[
mkdir -p ${INSTALL_DIR}/usr/local/share/hyper
cp build-clang-aarch64-uefi/loader/hyper_uefi ${INSTALL_DIR}/usr/local/share/hyper/BOOTAA64.EFI
cp build-clang-amd64-uefi/loader/hyper_uefi ${INSTALL_DIR}/usr/local/share/hyper/BOOTX64.EFI
cp build-clang-i686-bios/loader/hyper_iso_boot ${INSTALL_DIR}/usr/local/share/hyper/hyper_iso_boot

mkdir -p ${INSTALL_DIR}/usr/local/bin
cp installer/hyper_install ${INSTALL_DIR}/usr/local/bin/
        ]=],
    }
end
