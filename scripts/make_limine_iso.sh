#!/bin/sh

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <disk image file> <services> <config file name>"
    exit 1
fi

if [ -z "${SYSROOT+x}" ]; then
    echo "Error: $SYSROOT is not set"
    exit 1
fi

copy_service() {
    SERVICE="$1"
    cp -v "${BOOT_DIR}/${SERVICE}" "iso_root/${SERVICE}.elf"
    cp -v "${BOOT_DIR}/${SERVICE}.yaml" "iso_root/${SERVICE}.yaml"
}

BOOT_DIR="$SYSROOT/boot"

DISK_IMAGE="$1"
SERVICES="$2"
CONFIG_FILE="$3"

mkdir -p iso_root

cp -v "$BOOT_DIR/kernel" iso_root/
cp -v "$BOOT_DIR/bootstrapd" iso_root/

# Copy services
for SERVICE in $SERVICES; do
    copy_service "$SERVICE"
done

mkdir -p iso_root/EFI/BOOT
cp -v "/usr/local/share/limine/BOOT"* iso_root/EFI/BOOT/

mkdir -p iso_root/boot/limine
cp -v "/usr/local/share/limine/limine-bios.sys" iso_root/boot/limine/
cp -v "/usr/local/share/limine/limine-bios-cd.bin" iso_root/boot/limine/
cp -v "/usr/local/share/limine/limine-uefi-cd.bin" iso_root/boot/limine/

cp -v "$BOOT_DIR/$CONFIG_FILE" iso_root/boot/limine/

xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
    -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image --protective-msdos-label \
    iso_root -o "$DISK_IMAGE"

limine bios-install "$DISK_IMAGE"