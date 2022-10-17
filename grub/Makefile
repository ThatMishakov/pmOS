all: iso

clean:
	rm -rf isodir/ $(ISO)

ISO = pmOS.iso

iso: $(ISO)

../loader/loader.elf:
	$(MAKE) -C ../loader loader.elf

../kernel/kernel:
	$(MAKE) -C ../kernel kernel

../processd/processd.elf:
	$(MAKE) -C ../processd processd.elf

$(ISO): ../loader/loader.elf ../kernel/kernel ../processd/processd.elf
	mkdir -p isodir/boot/pmos
	mkdir -p isodir/boot/grub
	cp ../loader/loader.elf isodir/boot/pmos/
	cp ../kernel/kernel isodir/boot/pmos/
	cp ../processd/processd.elf isodir/boot/pmos/
	cp grub.cfg isodir/boot/grub/
	grub2-mkrescue -o $(ISO) isodir

.PHONY: ../loader/loader.elf ../kernel/kernel ../processd/processd.elf