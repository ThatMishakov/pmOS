TOPTARGETS := all clean

SUBDIRS := $(wildcard */.)

$(TOPTARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

emul: grub/pmOS.iso
	bochs-debugger -q -f .bochsrc

qemu: grub/pmOS.iso
	qemu-system-x86_64 -cdrom grub/pmOS.iso  -no-reboot -d cpu_reset

grub/pmOS.iso:
	$(MAKE) -C grub pmOS.iso

.PHONY: $(TOPTARGETS) $(SUBDIRS) grub/pmOS.iso
