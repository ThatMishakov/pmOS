ISO=build/pmOS.iso
X86_64-IMG=build-X86_64/pmos.img
RISCV64-IMG=build-RISCV64/pmos.img
LOONGARCH64-IMG=build-LOONGARCH64/pmos.img

jinx:
	wget -O jinx https://codeberg.org/Mintsuki/jinx/raw/commit/e6f44d1bd8c6a504fc3fbfcc16ddb549e2e89a3c/jinx
	chmod +x jinx

build-X86_64/.jinx-parameters: jinx
	@mkdir -p build-X86_64
	@cd build-X86_64 && ../jinx init .. ARCH=x86_64

build-RISCV64/.jinx-parameters: jinx
	@mkdir -p build-RISCV64
	@cd build-RISCV64 && ../jinx init .. ARCH=riscv64

build-LOONGARCH64/.jinx-parameters: jinx
	@mkdir -p build-LOONGARCH64
	@cd build-LOONGARCH64 && ../jinx init .. ARCH=loongarch64

$(X86_64-IMG): build-X86_64/.jinx-parameters
	@cd build-X86_64 && ../jinx build limine-disk-image

$(RISCV64-IMG): build-RISCV64/.jinx-parameters
	@cd build-RISCV64 && ../jinx build limine-disk-image

$(LOONGARCH64-IMG): build-LOONGARCH64/.jinx-parameters
	@cd build-LOONGARCH64 && ../jinx build limine-disk-image

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

qemu-x86: $(X86_64-IMG) ovmf-x86
	qemu-system-x86_64 \
		-drive file=$(X86_64-IMG),if=none,id=hdd0\
    	-device ide-hd,drive=hdd0 \
		-M q35\
		-smbios type=0,uefi=on -bios ovmf-x86/OVMF.fd\
		-m 256M\
	       	-smp 1\
	       	-serial stdio \
		-device intel-iommu -cpu max,x2apic=on,+smep,+smap -no-shutdown -no-reboot
# -trace ahci_* -trace handle_cmd_* \

qemu-kvm: $(ISO) ovmf-x86
	qemu-system-x86_64 -serial stdio -bios ovmf-x86/OVMF.fd -m 512M -cpu max,+hypervisor,+invtsc,+tsc-deadline -M q35 -accel kvm -cdrom limine/pmOS.iso -smp 1

qemu: $(RISCV64-IMG) ovmf-riscv64
	qemu-system-riscv64 -M virt -cpu rv64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-riscv64/OVMF.fd -device ahci,id=ahci -device ide-hd,drive=hdd0 -drive file=$(RISCV64-IMG),if=none,id=hdd0 -serial stdio -smp 4
	
qemu-loongarch64: $(LOONGARCH64-IMG) ovmf-loongarch64
	qemu-system-loongarch64 -M loongarch64-evb -cpu loongarch64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-loongarch64/OVMF.fd -device ahci,id=ahci -device ide-hd,drive=hdd0 -drive file=$(LOONGARCH64-IMG),if=none,id=hdd0 -serial stdio -smp 4

qemu-single: $(ISO) ovmf-riscv64
	qemu-system-riscv64 -M virt -cpu rv64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-riscv64/OVMF.fd -cdrom $(ISO) -serial stdio

.PHONY: $(IMG)
