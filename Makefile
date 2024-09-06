TOPTARGETS := all clean

SUBDIRS := bootstrapd devicesd kernel logd ns16550 piped processd terminald vfatfsd vfsd lib test ahcid

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

ovmf-x86: ovmf-x86/OVMF.fd

ovmf-x86/OVMF.fd:
	mkdir -p ovmf-x86
	cd ovmf-x86 && curl -o OVMF.fd https://retrage.github.io/edk2-nightly/bin/RELEASEX64_OVMF.fd

qemu-x86: $(ISO) ovmf-x86
	sudo qemu-system-x86_64 -enable-kvm \
		-device pcie-root-port,id=root_port1,port=0x10,chassis=1,multifunction=on,slot=1,bus=pcie.0,addr=0x10 \
		-device x3130-upstream,id=switch_upstream,bus=root_port1,addr=0x0 \
		-device xio3130-downstream,id=switch_downstream1,bus=switch_upstream,chassis=3,addr=0x1 \
		-device xio3130-downstream,id=switch_downstream2,bus=switch_upstream,chassis=4,addr=0x2 \
		-device pcie-pci-bridge,id=pci_bridge1,bus=switch_downstream1,addr=0x0 \
		-device pcie-pci-bridge,id=pci_bridge2,bus=switch_downstream2,addr=0x0 \
		-device e1000e,id=nic1,bus=pci_bridge2,addr=0x0 \
		-drive file=$(ISO),if=none,id=hdd0\
    	-device ide-hd,drive=hdd0 \
		-M q35\
		-smbios type=0,uefi=on -bios ovmf-x86/OVMF.fd\
		-m 2G\
	       	-d cpu_reset\
	       	-smp 4\
	       	-serial stdio
		# -trace ahci_* -trace handle_cmd_* \


qemu: $(ISO) ovmf-riscv64
	qemu-system-riscv64 -M virt -cpu rv64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-riscv64/OVMF.fd -device ahci,id=ahci -device ide-hd,drive=cd0,bus=ahci.0 -drive id=cd0,format=raw,file=limine/pmOS.iso -serial stdio -smp 8

qemu-single: $(ISO) ovmf-riscv64
	qemu-system-riscv64 -M virt -cpu rv64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-riscv64/OVMF.fd -cdrom $(ISO) -serial stdio

.PHONY: $(TOPTARGETS) $(SUBDIRS) limine/pmOS.iso
