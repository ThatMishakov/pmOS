#include <uefi.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/elf.h>

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

bool isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

size_t strspn(const char *str, const char *accept) {
    size_t len = 0;
    while (str[len] != '\0') {
        bool found = false;
        for (const char *a = accept; *a != '\0'; ++a) {
            if (str[len] == *a) {
                found = true;
                break;
            }
        }
        if (!found) {
            break;
        }
        ++len;
    }
    return len;
}

size_t strcspn(const char *str, const char *reject) {
    size_t len = 0;
    while (str[len] != '\0') {
        for (const char *a = reject; *a != '\0'; ++a) {
            if (str[len] == *a) {
                return len;
            }
        }
        ++len;
    }
    return len;
}

char * strpbrk(const char *str, const char *accept) {
    while (*str != '\0') {
        for (const char *a = accept; *a != '\0'; ++a) {
            if (*str == *a) {
                return (char *)str;
            }
        }
        ++str;
    }
    return NULL;
}

char * strncpy(char *dest, const char *src, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        dest[i] = src[i];
        if (src[i] == '\0') {
            return dest;
        }
    }
    return dest;
}

char * strtok(char *str, const char *delim) {
    static char *last = NULL;
    if (str != NULL) {
        last = str;
    } else if (last == NULL) {
        return NULL;
    }

    char *start = last;
    start += strspn(start, delim);
    last = start + strcspn(start, delim);
    if (*last != '\0') {
        *last = '\0';
        ++last;
    } else {
        last = NULL;
    }
    return start;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 != '\0' && *s2 != '\0') {
        if (*s1 != *s2) {
            return *s1 - *s2;
        }
        ++s1;
        ++s2;
    }
    return *s1 - *s2;
}

int strncmp(const char *s1, const char *s2, size_t len) {
    while (*s1 != '\0' && *s2 != '\0' && len > 0) {
        if (*s1 != *s2) {
            return *s1 - *s2;
        }
        ++s1;
        ++s2;
        --len;
    }
    return 0;
}

size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len] != '\0') {
        ++len;
    }
    return len;
}

void * memset(void *ptr, int value, size_t num) {
    unsigned char *p = ptr;
    for (size_t i = 0; i < num; ++i) {
        p[i] = value;
    }
    return ptr;
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

const char * kernel_param = "kernel";
const int kernel_param_len = 6;
const char * bootstrap_param = "bootstrap";
const int bootstrap_param_len = 9;
const char * load_param = "load";
const int load_param_len = 4;

enum {
    FILE_KERNEL,
    FILE_BOOTSTRAP,
    FILE_LOAD,
};

struct loaded_file {
    int type;
    char * path;
    char * args;
};

struct loaded_file *kernel_file = NULL;

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
        File->Close(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to get pmOSboot.cfg size\n\r");
        return Status;
    }

    char *FileBuffer;
    Status = gBS->AllocatePool(EfiLoaderData, FileSize + 1, (void **)&FileBuffer);
    if(EFI_ERROR(Status)) {
        File->Close(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to allocate memory for pmOSboot.cfg\n\r");
        return Status;
    }

    Status = File->Read(File, &FileSize, FileBuffer);
    if(EFI_ERROR(Status)) {
        gBS->FreePool(FileBuffer);
        File->Close(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to read pmOSboot.cfg\n\r");
        return Status;
    }
    FileBuffer[FileSize] = '\0';

    char *line = strtok(FileBuffer, "\n\r");
    int line_num = 0;
    while (line != NULL) {
        ++line_num;
        // Find next line
        char * current_line = line;
        line = strtok(NULL, "\n\r");

        // Skip leading whitespace
        current_line += strspn(current_line, " \t");

        // Skip empty lines and comments
        if(*current_line == '\0' || *current_line == '#') {
            line = strtok(NULL, "\n\r");
            continue;
        }

        // Parse key
        char *key = current_line;
        int len = strcspn(key, " \t");

        if (len == kernel_param_len && strncmp(kernel_param, key, len) == 0) {
            if (kernel_file != NULL) {
                gST->ConOut->OutputString(gST->ConOut, L"Warning: Ignoring duplicate kernel entry\n\r");
                continue;
            }
            
            // Parse path
            char *path = key + len;
            path += strspn(path, " \t=");

            int path_len = strcspn(path, " \t");
            if (path_len == 0) {
                gST->ConOut->OutputString(gST->ConOut, L"Warning: Ignoring malformed kernel entry\n\r");
                continue;
            }

            char *args = path + path_len;
            args += strspn(args, " \t");
            int args_len = strcspn(args, " \t");
            struct loaded_file *file = NULL;
            if (gBS->AllocatePool(EfiLoaderData, sizeof(struct loaded_file), (void**)&file) != EFI_SUCCESS) {
                gST->ConOut->OutputString(gST->ConOut, L"Warning: Failed to allocate memory for kernel entry\n\r");
                continue;
            }

            file->type = FILE_KERNEL;
            if (gBS->AllocatePool(EfiLoaderData, path_len + 1, (void **)&file->path) != EFI_SUCCESS) {
                gBS->FreePool(file);
                gST->ConOut->OutputString(gST->ConOut, L"Warning: Failed to allocate memory for kernel path\n\r");
                continue;
            }

            if (args_len > 0) {
                if (gBS->AllocatePool(EfiLoaderData, args_len + 1, (void**)&file->args) != EFI_SUCCESS) {
                    gBS->FreePool(file->path);
                    gBS->FreePool(file);
                    gST->ConOut->OutputString(gST->ConOut, L"Warning: Failed to allocate memory for kernel args\n\r");
                    continue;
                }
                strncpy(file->args, args, args_len);
                file->args[args_len] = '\0';
            } else {
                file->args = NULL;
            }

            strncpy(file->path, path, path_len);
            file->path[path_len] = '\0';

            kernel_file = file;
        }
    }

    File->Close(File);
    gBS->FreePool(FileBuffer);

    return EFI_SUCCESS;
}

EFI_STATUS load_kernel(EFI_FILE_PROTOCOL *SystemVolume) {
    if (kernel_file == NULL) {
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: No kernel entry found\n\r");
        return EFI_NOT_FOUND;
    }

    gST->ConOut->OutputString(gST->ConOut, L"Loading kernel executable...\n\r");

    EFI_STATUS Status;

    size_t filename_size = strlen(kernel_file->path);
    uint16_t utf16_filename[filename_size + 1];
    ascii_to_ucs2(kernel_file->path, utf16_filename, filename_size + 1);

    EFI_FILE_PROTOCOL *File;
    Status = SystemVolume->Open(SystemVolume, &File, utf16_filename, EFI_FILE_MODE_READ, 0);
    if(EFI_ERROR(Status)) {
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to open kernel at ");
        gST->ConOut->OutputString(gST->ConOut, utf16_filename);
        gST->ConOut->OutputString(gST->ConOut, L"\n\r");
        return Status;
    }

    size_t FileSize;
    Status = file_size(File, &FileSize);
    if(EFI_ERROR(Status)) {
        File->Close(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to get kernel file size\n\r");
        return Status;
    }

    ELF_64bit kernel_header;
    if (FileSize < sizeof(ELF_64bit)) {
        File->Close(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Kernel file too small\n\r");
        return EFI_LOAD_ERROR;
    }

    size_t HeaderSize = sizeof(ELF_64bit);
    Status = File->Read(File, &HeaderSize, &kernel_header);
    if(EFI_ERROR(Status)) {
        File->Close(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to read kernel file\n\r");
        return Status;
    }

    if (kernel_header.magic != 0x464c457f) {
        File->Close(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Kernel file not in ELF format\n\r");
        return EFI_LOAD_ERROR;
    }

    if (kernel_header.bitness != ELF_64BIT) {
        File->Close(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Kernel file not 64-bit\n\r");
        return EFI_LOAD_ERROR;
    }

    if (kernel_header.instr_set != ELF_INSTR_SET) {
        File->Close(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Kernel executable for wrong architecture\n\r");

        unsigned short buf[64];
        i16itoa(kernel_header.instr_set, buf);
        gST->ConOut->OutputString(gST->ConOut, L"ELF ISA: ");
        gST->ConOut->OutputString(gST->ConOut, buf);
        gST->ConOut->OutputString(gST->ConOut, L"\n\r");
        return EFI_LOAD_ERROR;
    }

    int kernel_pheader_entries = kernel_header.program_header_entries;
    ELF_PHeader_64 kernel_pheaders[kernel_pheader_entries];
    Status = File->SetPosition(File, kernel_header.program_header);
    if(EFI_ERROR(Status)) {
        File->Close(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to read kernel program headers\n\r");
        return Status;
    }

    size_t headers_size = kernel_header.prog_header_size * kernel_pheader_entries;
    Status = File->Read(File, &headers_size, kernel_pheaders);
    if(EFI_ERROR(Status)) {
        File->Close(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to read kernel program headers\n\r");
        return Status;
    }

    // Calculate the pages to be allocated
    size_t pages = 0;
    for (int i = 0; i < kernel_pheader_entries; ++i) {
        ELF_PHeader_64 * p = &kernel_pheaders[i];
        if (p->type == ELF_SEGMENT_LOAD) {
            uint64_t start = p->p_vaddr;    
            uint64_t end = p->p_vaddr + p->p_memsz;

            // Align start and end to page boundaries
            start &= ~0xfffUL;
            end = (end + 0xfffUL) & ~0xfffUL;

            pages += (end - start) >> 12;
        }
    }

    // Print the number of pages to be allocated
    unsigned short buf[64];
    i16itoa(pages, buf);
    gST->ConOut->OutputString(gST->ConOut, L"Allocating ");
    gST->ConOut->OutputString(gST->ConOut, buf);
    gST->ConOut->OutputString(gST->ConOut, L" pages for kernel\n\r");

    // Allocate contiguos memory region
    // Even if the final mapping will be sparse, pass a single "used by kernel" region to it
    EFI_PHYSICAL_ADDRESS kernel_phys_addr;
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, &kernel_phys_addr);
    if(EFI_ERROR(Status)) {
        File->Close(File);
        gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to allocate memory for kernel\n\r");
        return Status;
    }

    // Load the kernel
    uint64_t active_offset = 0;
    for (int i = 0; i < kernel_pheader_entries; ++i) {
        ELF_PHeader_64 * p = &kernel_pheaders[i];
        if (p->type == ELF_SEGMENT_LOAD) {
            uint64_t start = p->p_vaddr;    
            uint64_t end = p->p_vaddr + p->p_memsz;

            // Align start and end to page boundaries
            start &= ~0xfffUL;
            end = (end + 0xfffUL) & ~0xfffUL;

            // Copy the segment
            size_t segment_size = end - start;
            Status = File->SetPosition(File, p->p_offset);
            if(EFI_ERROR(Status)) {
                File->Close(File);
                gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to read kernel segment\n\r");
                return Status;
            }

            uint64_t voffset = p->p_vaddr - start;
            uint64_t offset = voffset + active_offset;
            Status = File->Read(File, &segment_size, (void *)(kernel_phys_addr + offset));
            if(EFI_ERROR(Status)) {
                File->Close(File);
                gST->ConOut->OutputString(gST->ConOut, L"Could not load pmOS: Failed to read kernel segment\n\r");
                return Status;
            }

            // Zero what is not loaded
            memset((void *)(kernel_phys_addr + active_offset), 0, voffset);
            memset((void *)(kernel_phys_addr + offset + segment_size), 0, end - (p->p_vaddr + p->p_memsz));

            // Map the segment
            // TODO
        }
    }

    File->Close(File);
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

    Status = load_kernel(Volume);
    if(EFI_ERROR(Status))
        return Status;

    
    SystemTable->BootServices->Stall(5 * 1000000);
    return EFI_SUCCESS;
}