#include <uefi.h>
#include <stddef.h>
#include <stdbool.h>

int main() {
    // Do nothing
}

EFI_BOOT_SERVICES *gBS;
EFI_SYSTEM_TABLE *gST;

void ascii_to_ucs2(const char *str, uint16_t *out, size_t max_len) {
    while (*str != '\0' && max_len > 1) {
        *out = *str;
        ++str;
        ++out;
        --max_len;
    }
    *out = 0;
}

void i16itoa(long n, unsigned short *out) {
    int pos = 0;
    unsigned short buff[24];

    if (n == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    if (n < 0) {
        n *= -1;
        *out = '-';
        ++out;
    }

    while (n > 0) {
        buff[pos] = '0' + n%10;
        n /= 10;
        ++pos;
    }

    for (int i = 0; i < pos; ++i) {
        out[i] = buff[pos - 1 - i];
    }

    out[pos] = '\0';
}

// https://wiki.osdev.org/Loading_files_under_UEFI
EFI_STATUS GetVolume(EFI_HANDLE image, EFI_FILE_PROTOCOL **Volume) {
    EFI_STATUS status = EFI_SUCCESS;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;                  /* image interface */
    EFI_GUID lipGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;               /* image interface GUID */
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *IOVolume;                       /* file system interface */
    EFI_GUID fsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;          /* file system interface GUID */

    /* get the loaded image protocol interface for our "image" */
    status = gBS->HandleProtocol(image, &lipGuid, (void **)&loaded_image);
    if (EFI_ERROR(status))
        return status;

    /* get the volume handle */
    status = gBS->HandleProtocol(loaded_image->DeviceHandle, &fsGuid, (void **)&IOVolume);
    if (EFI_ERROR(status))
        return status;
    status = IOVolume->OpenVolume(IOVolume, Volume);
    if (EFI_ERROR(status))
        return status;
    return status;
}

EFI_STATUS file_size(EFI_FILE_PROTOCOL *File, UINTN *Size) {
    EFI_STATUS Status;
    EFI_FILE_INFO *FileInfo;
    UINTN BufferSize = 0;
    EFI_GUID gEfiFileInfoGuid = EFI_FILE_INFO_ID;

    // Get file buffer size
    Status = File->GetInfo(File, &gEfiFileInfoGuid, &BufferSize, NULL);
    if(Status != EFI_BUFFER_TOO_SMALL)
        return Status;

    // Allocate buffer
    Status = gBS->AllocatePool(EfiLoaderData, BufferSize, (void **)&FileInfo);
    if(EFI_ERROR(Status))
        return Status;

    // Get file info
    Status = File->GetInfo(File, &gEfiFileInfoGuid, &BufferSize, FileInfo);
    if(EFI_ERROR(Status)) {
        gBS->FreePool(FileInfo);
        return Status;
    }

    // Return file size
    *Size = FileInfo->FileSize;
    gBS->FreePool(FileInfo);
    return EFI_SUCCESS;
}

EFI_STATUS parse_configs(EFI_HANDLE /* ImageHandle */, EFI_FILE_PROTOCOL *RootVolume) {
    EFI_STATUS Status;

    uint16_t *filename = L"\\pmOSboot.cfg";
    EFI_FILE_PROTOCOL *File;

    Status = RootVolume->Open(RootVolume, &File, filename, EFI_FILE_MODE_READ, 0);
    if(EFI_ERROR(Status)) {
        Status = gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to open pmOSboot.cfg\n\r");
        return Status;
    }

    size_t FileSize;
    Status = file_size(File, &FileSize);
    if(EFI_ERROR(Status)) {
        gBS->FreePool(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to get pmOSboot.cfg size\n\r");
        return Status;
    }

    char *FileBuffer;
    Status = gBS->AllocatePool(EfiLoaderData, FileSize, (void **)&FileBuffer);
    if(EFI_ERROR(Status)) {
        gBS->FreePool(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to allocate memory for pmOSboot.cfg\n\r");
        return Status;
    }

    Status = File->Read(File, &FileSize, FileBuffer);
    if(EFI_ERROR(Status)) {
        gBS->FreePool(FileBuffer);
        gBS->FreePool(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to read pmOSboot.cfg\n\r");
        return Status;
    }

    // Print back the ASCII file
    unsigned short *FileBufferUcs2;
    Status = gBS->AllocatePool(EfiLoaderData, FileSize * 2, (void **)&FileBufferUcs2);
    if(EFI_ERROR(Status)) {
        gBS->FreePool(FileBuffer);
        gBS->FreePool(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to allocate memory for pmOSboot.cfg\n\r");
        return Status;
    }
    ascii_to_ucs2(FileBuffer, FileBufferUcs2, FileSize);
    Status = gST->ConOut->OutputString(gST->ConOut, FileBufferUcs2);

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    (void)ImageHandle;
	EFI_STATUS Status;
	EFI_INPUT_KEY Key;

    gST = SystemTable;
    gBS = SystemTable->BootServices;

    Status = SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Loading pmOS...\n\r");
    if(EFI_ERROR(Status))
		return Status;

    EFI_FILE_PROTOCOL *Volume;
    Status = GetVolume(ImageHandle, &Volume);
    if(EFI_ERROR(Status))
        return Status;
    
    Status = parse_configs(ImageHandle, Volume);
    if(EFI_ERROR(Status))
        return Status;

    
    SystemTable->BootServices->Stall(5 * 1000000);
    return EFI_SUCCESS;
}