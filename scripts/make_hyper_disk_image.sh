if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <disk image file> <services> <config file name>"
    exit 1
fi

if [ -z "${SYSROOT+x}" ]; then
    echo "Error: $SYSROOT is not set"
    exit 1
fi

BOOT_DIR="$SYSROOT/boot"

DISK_IMAGE="$1"
SERVICES="$2"
CONFIG_FILE="$3"

BOOT_PARTITION=tmp_boot.img
EXT4_PARTITION=tmp_ext4.img

copy_service() {
    SERVICE="$1"
    echo "Copying service $SERVICE..."
    mcopy -i "$BOOT_PARTITION" "${BOOT_DIR}/${SERVICE}" "::/${SERVICE}.elf"
    mcopy -i "$BOOT_PARTITION" "${BOOT_DIR}/${SERVICE}.yaml" "::/${SERVICE}.yaml"
}

dd if=/dev/zero of="$DISK_IMAGE" bs=1M count=128

# Create an MBR partition table
echo 'label: dos' | sfdisk "$DISK_IMAGE"
echo '2048,130048,0x0C,*' | sfdisk --append "$DISK_IMAGE"
echo '132096,130048,0x83' | sfdisk --append "$DISK_IMAGE"

# Create a FAT32 filesystem for the boot partition
dd if=/dev/zero of="$BOOT_PARTITION" bs=1M count=64
mformat -i "$BOOT_PARTITION" -h 32 -t 32 -n 64 -c 1

# Copy config file
mcopy -i "$BOOT_PARTITION" "$BOOT_DIR/$CONFIG_FILE" "::/$CONFIG_FILE"

# Copy kernel
mcopy -i "$BOOT_PARTITION" "${BOOT_DIR}/kernel" "::/kernel"

# Copy bootstrapd
mcopy -i "$BOOT_PARTITION" "${BOOT_DIR}/bootstrapd" "::/bootstrapd"

# Copy hyper
mmd -i "$BOOT_PARTITION" "::/EFI"
mmd -i "$BOOT_PARTITION" "::/EFI/BOOT"
mcopy -i "$BOOT_PARTITION" "/usr/local/share/hyper/BOOTX64.EFI" "::/EFI/BOOT/BOOTX64.EFI"
mcopy -i "$BOOT_PARTITION" "/usr/local/share/hyper/BOOTAA64.EFI" "::/EFI/BOOT/BOOTAA64.EFI"

# Copy services
for SERVICE in $SERVICES; do
    copy_service "$SERVICE"
done

# Create an ext4 filesystem for the root partition
dd if=/dev/zero of="$EXT4_PARTITION" bs=1M count=64
mke2fs -d "$SYSROOT" -t ext4 "$EXT4_PARTITION" -L "pmos-root"

# Combine the partitions into the final disk image
dd conv=notrunc if="$BOOT_PARTITION" of="$DISK_IMAGE" bs=512 seek=2048
dd conv=notrunc if="$EXT4_PARTITION" of="$DISK_IMAGE" bs=512 seek=132096

hyper_install "$DISK_IMAGE"