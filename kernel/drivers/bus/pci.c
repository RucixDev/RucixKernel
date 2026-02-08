#include "drivers/pci.h"
#include "console.h"
#include "heap.h"
#include "string.h"

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ( "outl %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ( "inl %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ( "outw %0, %1" : : "a"(val), "Nd"(port) );
}

void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    
    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
 
    outl(PCI_CONFIG_ADDRESS, address);
    outw(PCI_CONFIG_DATA + (offset & 2), val);
}

static uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    
    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
 
    outl(PCI_CONFIG_ADDRESS, address);
    
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read_config(bus, slot, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

static uint8_t pci_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read_config(bus, slot, func, offset);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

struct bus_type pci_bus_type = {
    .name = "pci",
};

int pci_match(struct device *dev, struct device_driver *drv) {
    struct pci_device *pdev = to_pci_device(dev);
    struct pci_driver *pdrv = (struct pci_driver*)drv; 
    
     
    if (pdrv->class != 0 || pdrv->subclass != 0) {
        if (pdev->class_code == pdrv->class && pdev->subclass == pdrv->subclass) {
            return 1;
        }
    }
    
    if (pdrv->vendor_id == 0xFFFF) return 1; 
    
    if (pdrv->vendor_id == pdev->vendor_id) {
        if (pdrv->device_id == 0xFFFF || pdrv->device_id == pdev->device_id) {
            return 1;
        }
    }
    return 0;
}

static void pci_check_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor_id = pci_read_word(bus, device, function, 0);
    if (vendor_id == 0xFFFF) return;

    struct pci_device *dev = (struct pci_device*)kmalloc(sizeof(struct pci_device));
    if (!dev) return;
    
    memset(dev, 0, sizeof(struct pci_device));
    dev->bus = bus;
    dev->slot = device;
    dev->func = function;
    dev->vendor_id = vendor_id;
    dev->device_id = pci_read_word(bus, device, function, 2);
    dev->class_code = pci_read_byte(bus, device, function, 0xB);
    dev->subclass = pci_read_byte(bus, device, function, 0xA);
    dev->prog_if = pci_read_byte(bus, device, function, 0x9);
    dev->revision_id = pci_read_byte(bus, device, function, 0x8);
    dev->header_type = pci_read_byte(bus, device, function, 0xE);
    
    for (int i=0; i<6; i++) {
        dev->bar[i] = pci_read_config(bus, device, function, 0x10 + (i*4));
    }
    
    dev->interrupt_line = pci_read_byte(bus, device, function, 0x3C);
    
    int idx = 0;
    dev->dev.name[idx++] = 'p'; 
    dev->dev.name[idx++] = 'c'; 
    dev->dev.name[idx++] = 'i'; 
    dev->dev.name[idx++] = '_';
    
    dev->dev.name[idx++] = '0' + (bus % 10);
    dev->dev.name[idx++] = '.';
    dev->dev.name[idx++] = '0' + (device % 10);
    dev->dev.name[idx++] = '.';
    dev->dev.name[idx++] = '0' + (function % 10);
    dev->dev.name[idx] = 0;

    dev->dev.bus = &pci_bus_type;
    
    kprint_str("[PCI] Found ");
    kprint_hex(dev->vendor_id);
    kprint_str(":");
    kprint_hex(dev->device_id);
    kprint_str(" (");
    kprint_hex(bus);
    kprint_str(":");
    kprint_hex(device);
    kprint_str(".");
    kprint_hex(function);
    kprint_str(")\n");
    
    device_register(&dev->dev);
}

void pci_check_device(uint8_t bus, uint8_t device) {
    uint16_t vendor_id = pci_read_word(bus, device, 0, 0);
    if (vendor_id == 0xFFFF) return;

    pci_check_function(bus, device, 0);
    
    uint8_t header_type = pci_read_byte(bus, device, 0, 0xE);
    if (header_type & 0x80) {
        for (uint8_t f = 1; f < 8; f++) {
             pci_check_function(bus, device, f);
        }
    }
}

int pci_enable_device(struct pci_device *dev) {
    uint16_t cmd = pci_read_word(dev->bus, dev->slot, dev->func, 0x04);
     
    cmd |= 0x7;
    pci_write_word(dev->bus, dev->slot, dev->func, 0x04, cmd);
    
    kprint_str("[PCI] Device Enabled: ");
    kprint_hex(dev->vendor_id);
    kprint_str(":");
    kprint_hex(dev->device_id);
    kprint_newline();
    return 0;
}

void pci_scan_bus() {
    kprint_str("[PCI] Scanning bus...\n");
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            pci_check_device(bus, slot);
        }
    }
}

void pci_init(void) {
    pci_bus_type.match = pci_match;
    bus_register(&pci_bus_type);
    pci_scan_bus();
}

int pci_register_driver(struct pci_driver *driver) {
    driver->driver.bus = &pci_bus_type;
    return driver_register(&driver->driver);
}

void pci_unregister_driver(struct pci_driver *driver) {
    driver_unregister(&driver->driver);
}
