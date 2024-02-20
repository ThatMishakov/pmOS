TOPTARGETS := all clean

SUBDIRS := $(wildcard */.)

grub/pmOS.iso:
	$(MAKE) -C grub pmOS.iso

$(TOPTARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

ISO = kernel/build/image.iso

emul: $(ISO)
	bochs-debugger -q -f .bochsrc

bochs: $(ISO)
	bochs -q -f .bochsrc

qemu: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO)  -no-shutdown -d cpu_reset -smp 4 -serial stdio

.PHONY: $(TOPTARGETS) $(SUBDIRS) grub/pmOS.iso
