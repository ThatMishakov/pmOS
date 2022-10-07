#include <mem.h>
#include <multiboot2.h>
#include <entry.h>

void load_kernel(uint64_t multiboot_info_str)
{
    struct multiboot_tag_module * mod = 0;

    for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
      tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
        switch (tag->type) {
        case MULTIBOOT_TAG_TYPE_MODULE: {
            if (((struct multiboot_tag_module *)tag)->cmdline == "kernel")
                mod = (struct multiboot_tag_module *)tag;
        }
            break;
        default:
            break;
        }
      }

    if (!mod) {
        print_str("Did not find kernel module\n");
        return;
    }

    /* Parse elf */
}