all: iso

clean:
	rm -rf isodir/ $(ISO)

ISO = pmOS.iso

iso: $(ISO)

../kernel/build/kernel:
	$(MAKE) -C ../kernel build/kernel

../bootstrapd/bootstrapd.elf:
	$(MAKE) -C ../bootstrapd bootstrapd.elf

../devicesd/devicesd.elf:
	$(MAKE) -C ../devicesd devicesd.elf

../logd/logd.elf:
	$(MAKE) -C ../logd logd.elf

../ns16550/ns16550.elf:
	$(MAKE) -C ../ns16550 ns16550.elf

../piped/piped.elf:
	$(MAKE) -C ../piped piped.elf

../processd/processd.elf:
	$(MAKE) -C ../processd processd.elf

../terminald/terminald.elf:
	$(MAKE) -C ../terminald terminald.elf

../vfsd/vfsd.elf:
	$(MAKE) -C ../vfsd vfsd.elf

../vfatfsd/vfatfsd.elf:
	$(MAKE) -C ../vfatfsd vfatfsd.elf

../ahcid/ahcid.elf:
	$(MAKE) -C ../ahcid ahcid.elf

../ps2d/ps2d.elf:
	$(MAKE) -C ../ps2d ps2d.elf

../i8042/i8042.elf:
	$(MAKE) -C ../i8042 i8042.elf

../blockd/blockd.elf:
	$(MAKE) -C ../blockd blockd.elf

../test/test.elf:
	$(MAKE) -C ../test test.elf

limine: limine/.git

limine/.git:
	git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary --depth=1

limine-binary: limine limine/limine
	$(MAKE) -C limine

# isodir/boot/pmos/sysroot.tar: ../sysroot
# 	mkdir -p isodir/boot/pmos
# 	tar -cf isodir/boot/pmos/sysroot.tar -C ../sysroot .

DISK_IMAGE := pmos.img
BOOT_PARTITION := boot.img
EXT2_PARTITION := ext2.img

$(ISO): ../kernel/build/kernel ../bootstrapd/bootstrapd.elf ../devicesd/devicesd.elf ../logd/logd.elf ../ns16550/ns16550.elf ../piped/piped.elf ../processd/processd.elf ../terminald/terminald.elf ../vfsd/vfsd.elf ../vfatfsd/vfatfsd.elf ../ahcid/ahcid.elf ../ps2d/ps2d.elf ../i8042/i8042.elf ../blockd/blockd.elf limine.conf limine limine-binary
	mkdir -p isodir/boot/limine
	mkdir -p isodir/EFI/BOOT

# cp limine.conf isodir/boot/limine/
# cp -v limine/limine-uefi-cd.bin isodir/boot/limine/
# cp -v limine/limine-bios-cd.bin isodir/boot/limine/
# cp -v limine/limine-bios.sys isodir/boot/limine/
# cp -v limine/BOOTRISCV64.EFI isodir/EFI/BOOT/
# cp -v limine/BOOTX64.EFI isodir/EFI/BOOT/

# cp -v ../kernel/build/kernel isodir/kernel

# cp -v ../bootstrapd/bootstrapd.elf isodir/
# cp -v ../devicesd/devicesd.elf isodir/
# cp -v ../logd/logd.elf isodir/
# cp -v ../ns16550/ns16550.elf isodir/
# cp -v ../piped/piped.elf isodir/
# cp -v ../processd/processd.elf isodir/
# cp -v ../terminald/terminald.elf isodir/
# cp -v ../vfsd/vfsd.elf isodir/
# cp -v ../vfatfsd/vfatfsd.elf isodir/
# cp -v ../ahcid/ahcid.elf isodir/
# cp -v ../ps2d/ps2d.elf isodir/
# cp -v ../i8042/i8042.elf isodir/
# cp -v ../blockd/blockd.elf isodir/
# cp -v ../test/test.elf isodir

	dd if=/dev/zero of=$(DISK_IMAGE) bs=1M count=128

	# Create a GPT partition table
	sgdisk -og $(DISK_IMAGE)
	sgdisk -n 1:2048:67583 -c 1:"boot" -t 1:ef00 $(DISK_IMAGE)
	sgdisk -n 2:67584:$(shell expr 128 \* 1024 - 1) -c 2:"ext2" -t 2:8300 $(DISK_IMAGE)

	# Create a FAT32 filesystem for the boot partition
	dd if=/dev/zero of=$(BOOT_PARTITION) bs=1M count=64
	mformat -i $(BOOT_PARTITION) -h 32 -t 32 -n 64 -c 1
	mmd -i $(BOOT_PARTITION) ::/EFI
	mmd -i $(BOOT_PARTITION) ::/EFI/BOOT
	mmd -i $(BOOT_PARTITION) ::/limine

	mcopy -i $(BOOT_PARTITION) limine/limine-bios.sys ::/limine
	mcopy -i $(BOOT_PARTITION) limine/BOOTX64.EFI ::/EFI/BOOT
	mcopy -i $(BOOT_PARTITION) limine/BOOTRISCV64.EFI ::/EFI/BOOT

	mcopy -i $(BOOT_PARTITION) limine.conf ::/limine

	mcopy -i $(BOOT_PARTITION) ../kernel/build/kernel ::/kernel
	mcopy -i $(BOOT_PARTITION) ../bootstrapd/bootstrapd.elf ::/
	mcopy -i $(BOOT_PARTITION) ../devicesd/devicesd.elf ::/
	mcopy -i $(BOOT_PARTITION) ../logd/logd.elf ::/
	mcopy -i $(BOOT_PARTITION) ../ns16550/ns16550.elf ::/
	mcopy -i $(BOOT_PARTITION) ../piped/piped.elf ::/
	mcopy -i $(BOOT_PARTITION) ../processd/processd.elf ::/
	mcopy -i $(BOOT_PARTITION) ../terminald/terminald.elf ::/
	mcopy -i $(BOOT_PARTITION) ../vfsd/vfsd.elf ::/
	mcopy -i $(BOOT_PARTITION) ../vfatfsd/vfatfsd.elf ::/
	mcopy -i $(BOOT_PARTITION) ../ahcid/ahcid.elf ::/
	mcopy -i $(BOOT_PARTITION) ../ps2d/ps2d.elf ::/
	mcopy -i $(BOOT_PARTITION) ../i8042/i8042.elf ::/
	mcopy -i $(BOOT_PARTITION) ../blockd/blockd.elf ::/

	# Create an ext2 filesystem for the root partition
	dd if=/dev/zero of=$(EXT2_PARTITION) bs=1M count=64
	mke2fs -d isodir -t ext2 $(EXT2_PARTITION)

	# Copy partition images to the disk image
	dd conv=notrunc if=$(BOOT_PARTITION) of=$(DISK_IMAGE) bs=512 seek=2048
	dd conv=notrunc if=$(EXT2_PARTITION) of=$(DISK_IMAGE) bs=512 seek=67584

	# Install Limine stage 1
	limine/limine bios-install $(DISK_IMAGE)


.PHONY: ../kernel/build/kernel ../bootstrapd/bootstrapd.elf ../devicesd/devicesd.elf ../logd/logd.elf ../ns16550/ns16550.elf ../piped/piped.elf ../processd/processd.elf ../terminald/terminald.elf ../vfsd/vfsd.elf ../vfatfsd/vfatfsd.elf ../ahcid/ahcid.elf ../ps2d/ps2d.elf ../i8042/i8042.elf ../blockd/blockd.elf