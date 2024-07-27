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

../test/test.elf:
	$(MAKE) -C ../test test.elf

limine: limine/.git

limine/.git:
	git clone https://github.com/limine-bootloader/limine.git --branch=v7.7.2-binary --depth=1

# isodir/boot/pmos/sysroot.tar: ../sysroot
# 	mkdir -p isodir/boot/pmos
# 	tar -cf isodir/boot/pmos/sysroot.tar -C ../sysroot .

$(ISO): ../kernel/build/kernel ../bootstrapd/bootstrapd.elf ../devicesd/devicesd.elf ../logd/logd.elf ../ns16550/ns16550.elf ../piped/piped.elf ../processd/processd.elf ../terminald/terminald.elf ../vfsd/vfsd.elf ../vfatfsd/vfatfsd.elf limine.cfg limine
	mkdir -p isodir/boot/limine
	mkdir -p isodir/EFI/BOOT

	cp limine.cfg isodir/boot/limine/
	cp -v limine/limine-uefi-cd.bin isodir/boot/limine/
	cp -v limine/limine-bios-cd.bin isodir/boot/limine/
	cp -v limine/limine-bios.sys isodir/boot/limine/
	cp -v limine/BOOTRISCV64.EFI isodir/EFI/BOOT/
	cp -v limine/BOOTX64.EFI isodir/EFI/BOOT/

	cp -v ../kernel/build/kernel isodir/kernel

	cp -v ../bootstrapd/bootstrapd.elf isodir/
	cp -v ../devicesd/devicesd.elf isodir/
	cp -v ../logd/logd.elf isodir/
	cp -v ../ns16550/ns16550.elf isodir/
	cp -v ../piped/piped.elf isodir/
	cp -v ../processd/processd.elf isodir/
	cp -v ../terminald/terminald.elf isodir/
	cp -v ../vfsd/vfsd.elf isodir/
	cp -v ../vfatfsd/vfatfsd.elf isodir/
# cp -v ../test/test.elf isodir

	xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label isodir -o $(ISO) 

.PHONY: ../kernel/build/kernel ../bootstrapd/bootstrapd.elf ../devicesd/devicesd.elf ../logd/logd.elf ../ns16550/ns16550.elf ../piped/piped.elf ../processd/processd.elf ../terminald/terminald.elf ../vfsd/vfsd.elf ../vfatfsd/vfatfsd.elf