#include <uefi.h>

void main() {
    // Do nothing
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    (void)ImageHandle;
	EFI_STATUS Status;
	EFI_INPUT_KEY Key;

    Status = SystemTable->ConOut->OutputString(SystemTable->ConOut, u"Hello, world!\n\r");
    if(EFI_ERROR(Status))
		return Status;
    
    SystemTable->BootServices->Stall(5 * 1000000);
    return EFI_SUCCESS;
}