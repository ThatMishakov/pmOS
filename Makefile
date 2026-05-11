ISO=build/pmOS.iso
i686-IMG=build-i686/pmos-hyper.img
x86_64-IMG=build-x86_64/pmos.img
x86_64-HYPER-IMG=build-x86_64/pmos-hyper.img
riscv64-IMG=build-riscv64/pmos.img
loongarch64-IMG=build-loongarch64/pmos.img

# jinx:
# 	wget -O jinx https://codeberg.org/Mintsuki/jinx/raw/commit/e6f44d1bd8c6a504fc3fbfcc16ddb549e2e89a3c/jinx
# 	chmod +x jinx

build-x86_64/.jinx-parameters:
	@mkdir -p build-x86_64
	@cd build-x86_64 && ../jinx/jinx init .. ARCH=x86_64

build-riscv64/.jinx-parameters:
	@mkdir -p build-riscv64
	@cd build-riscv64 && ../jinx/jinx init .. ARCH=riscv64

build-loongarch64/.jinx-parameters:
	@mkdir -p build-loongarch64
	@cd build-loongarch64 && ../jinx/jinx init .. ARCH=loongarch64

build-i686/.jinx-parameters:
	@mkdir -p build-i686
	@cd build-i686 && ../jinx/jinx init .. ARCH=i686

$(x86_64-IMG): build-x86_64/.jinx-parameters
	@cd build-x86_64 && ../jinx/jinx build limine-disk-image

$(riscv64-IMG): build-riscv64/.jinx-parameters
	@cd build-riscv64 && ../jinx/jinx build limine-disk-image

$(loongarch64-IMG): build-loongarch64/.jinx-parameters
	@cd build-loongarch64 && ../jinx/jinx build limine-disk-image

$(i686-IMG): build-i686/.jinx-parameters
	@cd build-i686 && ../jinx/jinx build hyper-disk-image

$(x86_64-HYPER-IMG): build-x86_64/.jinx-parameters
	@cd build-x86_64 && ../jinx/jinx build hyper-disk-image

emul: $(x86_64-HYPER-IMG)
	bochs-debugger -q -f .bochsrc

# bochs: $(ISO)
# 	bochs -q -f .bochsrc

ovmf-riscv64: ovmf-riscv64/OVMF.fd

ovmf-riscv64/OVMF.fd:
	mkdir -p ovmf-riscv64
	cd ovmf-riscv64 && curl -o OVMF.fd https://retrage.github.io/edk2-nightly/bin/RELEASEriscv64_VIRT_CODE.fd && dd if=/dev/zero of=OVMF.fd bs=1 count=0 seek=33554432

ovmf-loongarch64: ovmf-loongarch64/OVMF.fd

ovmf-loongarch64/OVMF.fd:
	mkdir -p ovmf-loongarch64
	cd ovmf-loongarch64 && curl -o OVMF.fd https://retrage.github.io/edk2-nightly/bin/RELEASEloongarch64_QEMU_EFI.fd

ovmf-x86: ovmf-x86/OVMF.fd

ovmf-x86/OVMF.fd:
	mkdir -p ovmf-x86
	cd ovmf-x86 && curl -o OVMF.fd https://retrage.github.io/edk2-nightly/bin/RELEASEX64_OVMF.fd

qemu-x86-limine: $(x86_64-IMG) ovmf-x86
	qemu-system-x86_64 \
		-drive file=$(x86_64-IMG),if=none,id=hdd0\
    	-device ide-hd,drive=hdd0 \
		-M q35\
		-smbios type=0,uefi=on -bios ovmf-x86/OVMF.fd\
		-m 512M\
	       	-smp 1\
	       	-serial stdio \
		-device intel-iommu -cpu max,x2apic=on,+smep,+smap
# -trace ahci_* -trace handle_cmd_* \

qemu-x86: $(x86_64-HYPER-IMG) ovmf-x86
	qemu-system-x86_64 \
		-drive file=$(x86_64-HYPER-IMG),if=none,id=hdd0\
		-smbios type=0,uefi=on -bios ovmf-x86/OVMF.fd\
    	-device ide-hd,drive=hdd0 \
		-M q35\
		-m 512M\
	       	-smp 1\
	       	-serial stdio \
		-device intel-iommu -cpu max,x2apic=on,+smep,+smap,+fred -no-reboot

qemu-i686: $(i686-IMG) ovmf-x86
	qemu-system-i386 -serial stdio -m 512M -cpu max,+hypervisor -M q35 -hdd $(i686-IMG) -smp 4

qemu-kvm: $(x86_64-HYPER-IMG) ovmf-x86
	qemu-system-x86_64 -serial stdio -bios ovmf-x86/OVMF.fd -m 512M -cpu max,+hypervisor,+invtsc,+tsc-deadline -M q35 -accel kvm -hdd $(x86_64-HYPER-IMG) -smp 4

qemu: $(riscv64-IMG) ovmf-riscv64
	qemu-system-riscv64 -M virt -cpu rv64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-riscv64/OVMF.fd -device ahci,id=ahci -device ide-hd,drive=hdd0 -drive file=$(riscv64-IMG),if=none,id=hdd0 -serial stdio -smp 4
	
qemu-loongarch64: $(loongarch64-IMG) ovmf-loongarch64
	qemu-system-loongarch64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-loongarch64/OVMF.fd -device ahci,id=ahci -device ide-hd,drive=hdd0 -drive file=$(loongarch64-IMG),if=none,id=hdd0 -serial stdio -smp 4

qemu-single: $(ISO) ovmf-riscv64
	qemu-system-riscv64 -M virt -cpu rv64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-riscv64/OVMF.fd -cdrom $(ISO) -serial stdio

.PHONY: $(x86_64-IMG) $(riscv64-IMG) $(loongarch64-IMG) $(i686-IMG) $(x86_64-HYPER-IMG)
