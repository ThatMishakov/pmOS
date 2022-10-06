#ifndef GDT_H
#define GDT_H
#include "../../kernel/common/gdt.h"

void loadGDT(GDT_descriptor_m32* desc);

#endif