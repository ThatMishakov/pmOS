TOPTARGETS := all clean

SUBDIRS := $(wildcard */.)

$(TOPTARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

emul: grub/pmOS.iso
	bochs -q -f .bochsrc

grub/pmOS.iso:
	$(MAKE) -C grub pmOS.iso

.PHONY: $(TOPTARGETS) $(SUBDIRS)