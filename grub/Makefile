all: iso

clean:
	rm -rf isodir/ $(ISO)

ISO = pmOS.iso

iso: $(ISO)

../loader/loader.elf:
	$(MAKE) -C ../loader loader.elf

../kernel/kernel:
	$(MAKE) -C ../kernel kernel

$(ISO): ../loader/loader.elf ../kernel/kernel
	mkdir -p isodir/boot/pmos
	mkdir -p isodir/boot/grub
	cp ../loader/loader.elf isodir/boot/pmos/
	cp ../kernel/kernel isodir/boot/pmos/
	cp grub.cfg isodir/boot/grub/
	grub2-mkrescue -o $(ISO) isodir

.PHONY: ../loader/loader.elf ../kernel/kernel