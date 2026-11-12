.DEFAULT_GOAL := build-x86_64/pmos.img
ISO=build/pmOS.iso
i686-IMG=build-i686/pmos-hyper.img
x86_64-IMG=build-x86_64/pmos.img
x86_64-HYPER-IMG=build-x86_64/pmos-hyper.img
x86_64-i686-IMG=build-x86_64-i686/pmos.img
x86_64-i686-HYPER-IMG=build-x86_64-i686/pmos-hyper.img
x86_64-GRUB-ISO=build-x86_64/pmos-grub.iso
x86_64-HYPER-ISO=build-x86_64/pmos-hyper.iso
riscv64-IMG=build-riscv64/pmos.img
loongarch64-IMG=build-loongarch64/pmos.img
aarch64-IMG=build-aarch64/pmos.img
AARCH64_UEFI ?= edk2-aarch64-code.fd

# The Lua configuration owns dependencies and incremental rebuilds. Always ask
# Chariot to evaluate them before running QEMU, even when an image already exists.
CHARIOT ?= $(CURDIR)/.chariot-tools/bin/chariot
TOOLCHAIN ?= clang
CHARIOT_FLAGS ?=

define build_image
	@mkdir -p build-$(1)
	$(CHARIOT) install --force --arch $(1) -o toolchain=$(TOOLCHAIN) $(CHARIOT_FLAGS) $(2) build-$(1)/chariot-$(TOOLCHAIN)
	cp -T build-$(1)/chariot-$(TOOLCHAIN)/images/$(3) build-$(1)/$(3)
endef

# Chariot has one target architecture per graph. Export the normal amd64 kernel,
# then let the ia32 image import that binary as a content-hashed boot asset.
define build_compat_image
	@mkdir -p build-x86_64-i686
	$(CHARIOT) install --force --arch x86_64 -o toolchain=$(TOOLCHAIN) $(CHARIOT_FLAGS) kernel build-x86_64/kernel-$(TOOLCHAIN)
	$(CHARIOT) install --force --arch i686 -o toolchain=$(TOOLCHAIN) -o kernel-arch=x86_64 $(CHARIOT_FLAGS) $(1) build-x86_64-i686/chariot-$(TOOLCHAIN)
	cp build-x86_64-i686/chariot-$(TOOLCHAIN)/images/$(2) build-x86_64-i686/$(2)
endef

$(x86_64-IMG): FORCE | $(CHARIOT)
	$(call build_image,x86_64,limine-disk-image,pmos.img)

$(riscv64-IMG): FORCE | $(CHARIOT)
	$(call build_image,riscv64,limine-disk-image,pmos.img)

$(loongarch64-IMG): FORCE | $(CHARIOT)
	$(call build_image,loongarch64,limine-disk-image,pmos.img)

$(i686-IMG): FORCE | $(CHARIOT)
	$(call build_image,i686,hyper-disk-image,pmos-hyper.img)

$(x86_64-HYPER-IMG): FORCE | $(CHARIOT)
	$(call build_image,x86_64,hyper-disk-image,pmos-hyper.img)

$(x86_64-i686-HYPER-IMG): FORCE | $(CHARIOT)
	$(call build_compat_image,hyper-disk-image,pmos-hyper.img)

$(x86_64-i686-IMG): FORCE | $(CHARIOT)
	$(call build_compat_image,limine-disk-image,pmos.img)

$(x86_64-GRUB-ISO): FORCE | $(CHARIOT)
	$(call build_image,x86_64,grub-iso,pmos-grub.iso)

$(x86_64-HYPER-ISO): FORCE | $(CHARIOT)
	$(call build_image,x86_64,hyper-iso,pmos-hyper.iso)

$(aarch64-IMG): FORCE | $(CHARIOT)
	$(call build_image,aarch64,limine-disk-image,pmos.img)

.chariot-tools/bin/chariot $(CURDIR)/.chariot-tools/bin/chariot: scripts/install-chariot.sh
	./scripts/install-chariot.sh

.PHONY: FORCE
FORCE:

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
qemu-x86-compat-limine: x86_64-IMG = $(x86_64-i686-IMG)
qemu-x86-compat-limine: $(x86_64-i686-IMG) ovmf-x86
qemu-x86-limine qemu-x86-compat-limine:
	qemu-system-x86_64 \
		-drive file=$(x86_64-IMG),if=none,id=hdd0\
    	-device ide-hd,drive=hdd0 \
		-M q35\
		-smbios type=0,uefi=on -bios ovmf-x86/OVMF.fd\
		-m 512M\
	       	-smp 1\
	       	-serial stdio \
		-device intel-iommu -no-reboot -no-shutdown -cpu max
# -trace ahci_* -trace handle_cmd_* \

qemu-x86: $(x86_64-HYPER-IMG) ovmf-x86
qemu-x86-compat: x86_64-HYPER-IMG = $(x86_64-i686-HYPER-IMG)
qemu-x86-compat: $(x86_64-i686-HYPER-IMG) ovmf-x86
qemu-x86 qemu-x86-compat:
	qemu-system-x86_64 \
		-drive file=$(x86_64-HYPER-IMG),if=none,id=hdd0\
		-smbios type=0,uefi=on -bios ovmf-x86/OVMF.fd\
    	-device ide-hd,drive=hdd0 \
		-M q35\
		-m 512M\
	       	-smp 1\
	       	-serial stdio \
		-device intel-iommu -cpu max,x2apic=on,+smep,+smap,+fred -no-reboot -smp 1

qemu-x86-gdb: $(x86_64-HYPER-IMG) ovmf-x86
	qemu-system-x86_64 \
		-drive file=$(x86_64-HYPER-IMG),if=none,id=hdd0\
		-smbios type=0,uefi=on -bios ovmf-x86/OVMF.fd\
		-device ide-hd,drive=hdd0 \
		-M q35\
		-m 512M\
	       	-smp 1\
	       	-serial stdio \
		-device intel-iommu -cpu max,x2apic=on,+smep,+smap,+fred -no-reboot -smp 2 -S -s

qemu-i686: $(i686-IMG) ovmf-x86
	qemu-system-i386 -serial stdio -m 512M -cpu max,+hypervisor -M q35 -hdd $(i686-IMG) -smp 4

qemu-kvm: $(x86_64-HYPER-IMG) ovmf-x86
	qemu-system-x86_64 -serial stdio -bios ovmf-x86/OVMF.fd -m 512M -cpu max,+hypervisor,+invtsc,+tsc-deadline -M q35 -accel kvm -hdd $(x86_64-HYPER-IMG) -smp 4

qemu: $(riscv64-IMG) ovmf-riscv64
	qemu-system-riscv64 -M virt -cpu rv64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-riscv64/OVMF.fd -device ahci,id=ahci -device ide-hd,drive=hdd0 -drive file=$(riscv64-IMG),if=none,id=hdd0 -serial stdio -smp 4
	
qemu-loongarch64: $(loongarch64-IMG) ovmf-loongarch64
	qemu-system-loongarch64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-loongarch64/OVMF.fd -device ahci,id=ahci -device ide-hd,drive=hdd0 -drive file=$(loongarch64-IMG),if=none,id=hdd0 -serial stdio -smp 4

.PHONY: qemu-aarch64
qemu-aarch64: $(aarch64-IMG)
	qemu-system-aarch64 \
		-M virt -cpu cortex-a72 -m 2G \
		-bios "$(AARCH64_UEFI)" \
		-device ramfb -device qemu-xhci -device usb-kbd \
		-drive if=none,id=disk,format=raw,file="$(aarch64-IMG)" \
		-device virtio-blk-device,drive=disk,bootindex=0 \
		-serial stdio

qemu-single: $(riscv64-IMG) ovmf-riscv64
	qemu-system-riscv64 -M virt -cpu rv64 -device ramfb -device virtio-keyboard -device qemu-xhci -device usb-kbd -m 2G -drive if=pflash,unit=0,format=raw,file=ovmf-riscv64/OVMF.fd -device ahci,id=ahci -device ide-hd,drive=hdd0 -drive file=$(riscv64-IMG),if=none,id=hdd0 -serial stdio

# .PHONY: $(x86_64-IMG) $(riscv64-IMG) $(loongarch64-IMG) $(i686-IMG) $(x86_64-HYPER-IMG) $(x86_64-GRUB-ISO) $(x86_64-HYPER-ISO)
