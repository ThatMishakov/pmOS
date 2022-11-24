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

$(ISO): ../loader/loader.elf ../kernel/kernel ../processd/processd.elf ../terminald/terminald.elf ../devicesd/devicesd.elf
	mkdir -p isodir/boot/pmos
	mkdir -p isodir/boot/grub
	cp ../loader/loader.elf isodir/boot/pmos/
	cp ../kernel/kernel isodir/boot/pmos/
	cp ../processd/processd.elf isodir/boot/pmos/
	cp ../terminald/terminald.elf isodir/boot/pmos/
	cp ../devicesd/devicesd.elf isodir/boot/pmos/
	cp grub.cfg isodir/boot/grub/
	grub2-mkrescue -o $(ISO) isodir

.PHONY: ../loader/loader.elf ../kernel/kernel ../processd/processd.elf ../terminald/terminald.elf ../devicesd/devicesd.elf