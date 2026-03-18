TOPTARGETS := all clean

SUBDIRS := sysroot lib bootstrapd devicesd kernel logd ns16550 piped processd terminald vfatfsd vfsd test ps2d blockd ahcid

# ifeq ($(filter i%86 x86,$(ARCH)),)
# 	SUBDIRS += i8042
# endif

limine/pmOS.iso:
	$(MAKE) -C limine pmOS.iso

limine/pmos.img:
	$(MAKE) -C limine pmos.img

$(TOPTARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

ISO = limine/pmOS.iso
IMG = limine/pmos.img

emul: $(ISO)
	bochs-debugger -q -f .bochsrc

# bochs: $(ISO)
# 	bochs -q -f .bochsrc

ovmf-riscv64: ovmf-riscv64/OVMF.fd

ovmf-riscv64/OVMF.fd:
	mkdir -p ovmf-riscv64
	cd ovmf-riscv64 && curl -o OVMF.fd https://retrage.github.io/edk2-nightly/bin/RELEASERISCV64_VIRT_CODE.fd && dd if=/dev/zero of=OVMF.fd bs=1 count=0 seek=33554432

ovmf-loongarch64: ovmf-loongarch64/OVMF.fd

ovmf-loongarch64/OVMF.fd:
	mkdir -p ovmf-loongarch64
	cd ovmf-loongarch64 && curl -o OVMF.fd https://retrage.github.io/edk2-nightly/bin/RELEASELOONGARCH64_QEMU_EFI.fd && dd if=/dev/zero of=OVMF.fd bs=1 count=0 seek=33554432

ovmf-x86: ovmf-x86/OVMF.fd

ovmf-x86/OVMF.fd:
	mkdir -p ovmf-x86
	cd ovmf-x86 && curl -o OVMF.fd https://retrage.github.io/edk2-nightly/bin/RELEASEX64_OVMF.fd

qemu-x86: $(IMG) ovmf-x86
	qemu-system-x86_64 \
		-drive file=$(IMG),if=none,id=hdd0\
    	-device ide-hd,drive=hdd0 \
		-M q35\
		-smbios type=0,uefi=on -bios ovmf-x86/OVMF.fd\
		-m 256M\
	       	-smp 1\
	       	-serial stdio \
		-device intel-iommu -cpu max,x2apic=on -no-shutdown -no-reboot
# -trace ahci_* -trace handle_cmd_* \

qemu-kvm: $(ISO) ovmf-x86
	qemu-system-x86_64 -serial stdio -bios ovmf-x86/OVMF.fd -m 512M -cpu max,+hypervisor,+invtsc,+tsc-deadline -M q35 -accel kvm -cdrom limine/pmOS.iso -smp 1


qemu: $(ISO) ovmf-riscv64
	qemu-system-riscv64 -M virt -cpu rv64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-riscv64/OVMF.fd -device ahci,id=ahci -device ide-hd,drive=hdd0 -drive file=$(ISO),if=none,id=hdd0 -serial stdio
	
qemu-single: $(ISO) ovmf-riscv64
	qemu-system-riscv64 -M virt -cpu rv64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-riscv64/OVMF.fd -cdrom $(ISO) -serial stdio

.PHONY: $(TOPTARGETS) $(SUBDIRS) limine/pmOS.iso limine/pmos.img
