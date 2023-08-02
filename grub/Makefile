all: iso

clean:
	rm -rf isodir/ $(ISO)

ISO = pmOS.iso

iso: $(ISO)

../loader/loader.elf:
	$(MAKE) -C ../loader loader.elf

../terminald/terminald.elf:
	$(MAKE) -C ../terminald terminald.elf

../kernel/kernel:
	$(MAKE) -C ../kernel kernel

../processd/processd.elf:
	$(MAKE) -C ../processd processd.elf

../devicesd/devicesd.elf:
	$(MAKE) -C ../devicesd devicesd.elf

../i8042/i8042.elf:
	$(MAKE) -C ../i8042 i8042.elf

../ps2d/ps2d.elf:
	$(MAKE) -C ../ps2d ps2d.elf

../ustarfsd/ustarfsd.elf:
	$(MAKE) -C ../ustarfsd ustarfsd.elf

../vfsd/vfsd.elf:
	$(MAKE) -C ../vfsd vfsd.elf

../vfatfsd/vfatfsd.elf:
	$(MAKE) -C ../vfatfsd vfatfsd.elf

isodir/boot/pmos/sysroot.tar: ../sysroot
	mkdir -p isodir/boot/pmos
	tar -cf isodir/boot/pmos/sysroot.tar -C ../sysroot .

$(ISO): ../loader/loader.elf ../kernel/kernel ../processd/processd.elf ../terminald/terminald.elf ../devicesd/devicesd.elf ../i8042/i8042.elf ../ps2d/ps2d.elf ../ustarfsd/ustarfsd.elf ../vfsd/vfsd.elf ../vfatfsd/vfatfsd.elf grub.cfg isodir/boot/pmos/sysroot.tar
	mkdir -p isodir/boot/pmos
	mkdir -p isodir/boot/grub
	cp ../loader/loader.elf isodir/boot/pmos/
	cp ../kernel/kernel isodir/boot/pmos/
	cp ../processd/processd.elf isodir/boot/pmos/
	cp ../terminald/terminald.elf isodir/boot/pmos/
	cp ../devicesd/devicesd.elf isodir/boot/pmos/
	cp ../i8042/i8042.elf isodir/boot/pmos/
	cp ../ps2d/ps2d.elf isodir/boot/pmos/
	cp ../ustarfsd/ustarfsd.elf isodir/boot/pmos/
	cp ../vfsd/vfsd.elf isodir/boot/pmos/
	cp ../vfatfsd/vfatfsd.elf isodir/boot/pmos/
	cp grub.cfg isodir/boot/grub/
	grub2-mkrescue -o $(ISO) isodir

.PHONY: ../loader/loader.elf ../kernel/kernel ../processd/processd.elf ../terminald/terminald.elf ../devicesd/devicesd.elf ../i8042/i8042.elf ../ps2d/ps2d.elf ../ustarfsd/ustarfsd.elf ../vfsd/vfsd.elf ../vfatfsd/vfatfsd.elf