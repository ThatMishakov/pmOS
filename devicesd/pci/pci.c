#include <pci/pci.h>

// TODO: Memory-mapped registers & stuff

void init_pci();

uint8_t pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset);
uint16_t pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset);
uint32_t pci_readl(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset);

void pci_writeb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint8_t val);
void pci_writew(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint16_t val);
void pci_writel(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint32_t val);