all: iso

clean:
	rm -rf isodir/ $(ISO)

ISO = pmOS.iso

iso: $(ISO)

../loader/loader.elf:
	$(MAKE) -C ../loader loader.elf

$(ISO): ../loader/loader.elf
	mkdir -p isodir/boot/pmos
	mkdir -p isodir/boot/grub
	cp ../loader/loader.elf isodir/boot/pmos/
	cp grub.cfg isodir/boot/grub/
	grub2-mkrescue -o $(ISO) isodir

.PHONY: ../loader/loader.elf