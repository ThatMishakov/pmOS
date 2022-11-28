TOPTARGETS := all clean

SUBDIRS := $(wildcard */.)

grub/pmOS.iso:
	$(MAKE) -C grub pmOS.iso

$(TOPTARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

emul: grub/pmOS.iso
	bochs-debugger -q -f .bochsrc

bochs: grub/pmOS.iso
	bochs -q -f .bochsrc

qemu: grub/pmOS.iso
	qemu-system-x86_64 -cdrom grub/pmOS.iso  -no-reboot -d cpu_reset -smp 4

.PHONY: $(TOPTARGETS) $(SUBDIRS) grub/pmOS.iso
