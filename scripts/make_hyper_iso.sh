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
cp -v "/usr/local/share/hyper/BOOT"* iso_root/EFI/BOOT/
cp -v "/usr/local/share/hyper/hyper_iso_boot" iso_root/

dd if=/dev/zero of="fat32_image.img" bs=1M count=2
mformat -i "fat32_image.img" -h 32 -t 32 -n 2 -c 1
mmd -i "fat32_image.img" "::/EFI"
mmd -i "fat32_image.img" "::/EFI/BOOT"
mcopy -i "fat32_image.img" "/usr/local/share/hyper/BOOTX64.EFI" "::/EFI/BOOT/BOOTX64.EFI"
mcopy -i "fat32_image.img" "/usr/local/share/hyper/BOOTAA64.EFI" "::/EFI/BOOT/BOOTAA64.EFI"

cp -v "fat32_image.img" iso_root/uefi_boot.img

cp -v "$BOOT_DIR/$CONFIG_FILE" iso_root/

xorriso -as mkisofs \
  -b hyper_iso_boot \
  -no-emul-boot \
  -boot-load-size 4 \
  -boot-info-table \
  --efi-boot uefi_boot.img \
  -efi-boot-part --efi-boot-image \
  --protective-msdos-label iso_root \
  -o "$DISK_IMAGE"

hyper_install "$DISK_IMAGE"