TOPTARGETS := all clean

SUBDIRS := bootstrapd devicesd kernel logd ns16550 piped processd terminald vfatfsd vfsd lib

limine/pmOS.iso:
	$(MAKE) -C limine pmOS.iso

$(TOPTARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

ISO = limine/pmOS.iso

# emul: $(ISO)
# 	bochs-debugger -q -f .bochsrc

# bochs: $(ISO)
# 	bochs -q -f .bochsrc

ovmf-riscv64: ovmf-riscv64/OVMF.fd

ovmf-riscv64/OVMF.fd:
	mkdir -p ovmf-riscv64
	cd ovmf-riscv64 && curl -o OVMF.fd https://retrage.github.io/edk2-nightly/bin/RELEASERISCV64_VIRT_CODE.fd && dd if=/dev/zero of=OVMF.fd bs=1 count=0 seek=33554432


qemu: $(ISO) ovmf-riscv64
	qemu-system-riscv64 -M virt -cpu rv64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-riscv64/OVMF.fd -device virtio-scsi-pci,id=scsi -device scsi-cd,drive=cd0 -drive id=cd0,format=raw,file=$(ISO) -serial stdio -smp 8

qemu-single: $(ISO) ovmf-riscv64
	qemu-system-riscv64 -M virt -cpu rv64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-riscv64/OVMF.fd -device virtio-scsi-pci,id=scsi -device scsi-cd,drive=cd0 -drive id=cd0,format=raw,file=$(ISO) -serial stdio

.PHONY: $(TOPTARGETS) $(SUBDIRS) limine/pmOS.iso
