#include "drm/drm.h"
#include "drivers/pci.h"
#include "io.h"
#include "heap.h"
#include "vmm.h"
#include "console.h"
#include "string.h"

#define BGA_PCI_VENDOR 0x1234
#define BGA_PCI_DEVICE 0x1111

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

#define VBE_DISPI_INDEX_ID          0
#define VBE_DISPI_INDEX_XRES        1
#define VBE_DISPI_INDEX_YRES        2
#define VBE_DISPI_INDEX_BPP         3
#define VBE_DISPI_INDEX_ENABLE      4
#define VBE_DISPI_INDEX_BANK        5
#define VBE_DISPI_INDEX_VIRT_WIDTH  6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 7
#define VBE_DISPI_INDEX_X_OFFSET    8
#define VBE_DISPI_INDEX_Y_OFFSET    9

#define VBE_DISPI_DISABLED          0x00
#define VBE_DISPI_ENABLED           0x01
#define VBE_DISPI_LFB_ENABLED       0x40

static void bga_write(uint16_t index, uint16_t data) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, data);
}

static uint16_t bga_read(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

struct bga_device {
    struct drm_device drm;
    struct pci_device *pdev;
    
    struct drm_crtc crtc;
    struct drm_connector connector;
    struct drm_encoder encoder;
    struct drm_framebuffer fb;
    
    uint64_t fb_phys;
    uint64_t fb_virt;
    uint32_t fb_size;
};

static void bga_crtc_dpms(struct drm_crtc *crtc, int mode) {
     
    if (mode == DRM_MODE_TYPE_DEFAULT) {  
        bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    } else {
        bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    }
}

static int bga_crtc_set_config(struct drm_crtc *crtc, struct drm_framebuffer *fb) {
    struct bga_device *bga = (struct bga_device *)crtc->dev->dev_private;
    
     
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write(VBE_DISPI_INDEX_XRES, fb->width);
    bga_write(VBE_DISPI_INDEX_YRES, fb->height);
    bga_write(VBE_DISPI_INDEX_BPP, fb->bpp);
    bga_write(VBE_DISPI_INDEX_VIRT_WIDTH, fb->width);
    bga_write(VBE_DISPI_INDEX_VIRT_HEIGHT, fb->height * 2);  
    bga_write(VBE_DISPI_INDEX_X_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_Y_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    
    crtc->primary = fb;
    return 0;
}

static const struct drm_crtc_funcs bga_crtc_funcs = {
    .dpms = bga_crtc_dpms,
    .set_config = bga_crtc_set_config,
};

static const struct drm_connector_funcs bga_connector_funcs = {
     
};

static const struct drm_encoder_funcs bga_encoder_funcs = {
     
};

static int bga_load(struct drm_device *dev, unsigned long flags) {
    (void)flags;
    struct bga_device *bga = (struct bga_device*)dev->dev_private;
 
    bga_write(VBE_DISPI_INDEX_ID, 0xB0C5);  

    bga->fb.width = 1024;
    bga->fb.height = 768;
    bga->fb.bpp = 32;
    bga->fb.pitch = 1024 * 4;
    bga->fb.dev = dev;
 
    bga->fb_phys = (uint64_t)bga->pdev->bar[0] & ~0xF;
    bga->fb_size = 16 * 1024 * 1024;  
    bga->fb_virt = (uint64_t)ioremap(bga->fb_phys, bga->fb_size);
    
    bga->fb.paddr = bga->fb_phys;
    bga->fb.vaddr = (void*)bga->fb_virt;
    
    drm_crtc_init(dev, &bga->crtc, &bga_crtc_funcs);

    drm_connector_init(dev, &bga->connector, &bga_connector_funcs);
    drm_encoder_init(dev, &bga->encoder, &bga_encoder_funcs);
    
    bga_crtc_set_config(&bga->crtc, &bga->fb);
    
    kprint_str("[BGA] Initialized 1024x768x32\n");
    return 0;
}

static struct drm_driver bga_driver = {
    .name = "bochs-drm",
    .desc = "Bochs Dispi VBE Driver",
    .load = bga_load,
};

static int bga_pci_probe(struct device *dev) {
    struct pci_device *pdev = to_pci_device(dev);
    
    if (pdev->vendor_id != BGA_PCI_VENDOR || pdev->device_id != BGA_PCI_DEVICE) {
        return -1;
    }
    
    pci_enable_device(pdev);
    
    struct bga_device *bga = (struct bga_device*)kmalloc(sizeof(struct bga_device));
    memset(bga, 0, sizeof(struct bga_device));
    
    bga->pdev = pdev;
    bga->drm.dev_private = bga;
    bga->drm.driver = &bga_driver;
    
    drm_dev_alloc(&bga_driver, dev);  
     
    INIT_LIST_HEAD(&bga->drm.crtc_list);
    INIT_LIST_HEAD(&bga->drm.connector_list);
    INIT_LIST_HEAD(&bga->drm.encoder_list);
    INIT_LIST_HEAD(&bga->drm.fb_list);
    
    drm_dev_register(&bga->drm, 0);
    
    return 0;
}

static struct pci_driver bga_pci_driver = {
    .vendor_id = BGA_PCI_VENDOR,
    .device_id = BGA_PCI_DEVICE,
    .driver = {
        .name = "bochs-vbe",
        .probe = bga_pci_probe,
    }
};

void bga_init(void) {
    pci_register_driver(&bga_pci_driver);
}
