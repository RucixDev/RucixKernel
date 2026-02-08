#include "drivers/usb/xhci.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "vmm.h"
#include "io.h"

static struct xhci_host *xhci_host = 0;

static uint32_t xhci_read32(struct xhci_host *xhci, uint64_t offset) {
    return *((volatile uint32_t*)(xhci->mmio_base + offset));
}

static void xhci_write32(struct xhci_host *xhci, uint64_t offset, uint32_t val) {
    *((volatile uint32_t*)(xhci->mmio_base + offset)) = val;
}

static uint64_t xhci_read64(struct xhci_host *xhci, uint64_t offset) {
    return *((volatile uint64_t*)(xhci->mmio_base + offset));
}

static void xhci_write64(struct xhci_host *xhci, uint64_t offset, uint64_t val) {
    *((volatile uint64_t*)(xhci->mmio_base + offset)) = val;
}

static void xhci_reset(struct xhci_host *xhci) {
     
    uint32_t cmd = xhci_read32(xhci, xhci->op_base + XHCI_USBCMD);
    cmd &= ~XHCI_CMD_RS;
    xhci_write32(xhci, xhci->op_base + XHCI_USBCMD, cmd);
    
    while (!(xhci_read32(xhci, xhci->op_base + XHCI_USBSTS) & XHCI_STS_HCH));
    
     
    cmd |= XHCI_CMD_HCRST;
    xhci_write32(xhci, xhci->op_base + XHCI_USBCMD, cmd);
    
    while (xhci_read32(xhci, xhci->op_base + XHCI_USBCMD) & XHCI_CMD_HCRST);
    while (xhci_read32(xhci, xhci->op_base + XHCI_USBSTS) & XHCI_STS_CNR);
}

static void xhci_init_memory(struct xhci_host *xhci) {
     
    xhci->dcbaa = (uint64_t*)kmalloc(2048);  
    memset(xhci->dcbaa, 0, 2048);
    xhci->dcbaa_phys = vmm_get_phys((uint64_t)xhci->dcbaa);
    
    xhci_write64(xhci, xhci->op_base + XHCI_DCBAAP, xhci->dcbaa_phys);
    
     
    xhci->cmd_ring = (struct xhci_trb*)kmalloc(4096);
    memset(xhci->cmd_ring, 0, 4096);
    xhci->cmd_ring_phys = vmm_get_phys((uint64_t)xhci->cmd_ring);
    
    xhci_write64(xhci, xhci->op_base + XHCI_CRCR, xhci->cmd_ring_phys | 1);  
    
     
    xhci->event_ring = (struct xhci_trb*)kmalloc(4096);
    memset(xhci->event_ring, 0, 4096);
    xhci->event_ring_phys = vmm_get_phys((uint64_t)xhci->event_ring);
    
     
    xhci->erst = (struct xhci_erst_entry*)kmalloc(sizeof(struct xhci_erst_entry));
    xhci->erst->base = xhci->event_ring_phys;
    xhci->erst->size = 4096 / 16;  
    xhci->erst_phys = vmm_get_phys((uint64_t)xhci->erst);
    
    xhci_write32(xhci, xhci->rt_base + XHCI_ERSTSZ(0), 1);
    xhci_write64(xhci, xhci->rt_base + XHCI_ERSTBA(0), xhci->erst_phys);
    xhci_write64(xhci, xhci->rt_base + XHCI_ERDP(0), xhci->event_ring_phys);
    
     
    xhci_write32(xhci, xhci->rt_base + XHCI_IMAN(0), 3);  
    xhci_write32(xhci, xhci->rt_base + XHCI_IMOD(0), 4000);  
    
    xhci->cmd_ring_cycle = 1;
    xhci->event_ring_cycle = 1;
    xhci->cmd_ring_enqueue_idx = 0;
    xhci->event_ring_dequeue_idx = 0;
}

static int xhci_send_command(struct xhci_host *xhci, uint32_t param1, uint32_t param2, uint32_t status, uint32_t type) {
    uint32_t idx = xhci->cmd_ring_enqueue_idx;
    struct xhci_trb *trb = &xhci->cmd_ring[idx];
    
    trb->param = (uint64_t)param1 | ((uint64_t)param2 << 32);
    trb->status = status;
    trb->control = TRB_TYPE(type) | (xhci->cmd_ring_cycle ? TRB_C : 0);
    
    xhci->cmd_ring_enqueue_idx++;
    if (xhci->cmd_ring_enqueue_idx >= 4096 / sizeof(struct xhci_trb) - 1) {
         
        struct xhci_trb *link = &xhci->cmd_ring[xhci->cmd_ring_enqueue_idx];
        link->param = xhci->cmd_ring_phys;
        link->status = 0;
        link->control = TRB_TYPE(TRB_LINK) | TRB_TC | (xhci->cmd_ring_cycle ? TRB_C : 0);
        
        xhci->cmd_ring_cycle = !xhci->cmd_ring_cycle;
        xhci->cmd_ring_enqueue_idx = 0;
    }
    
     
    xhci_write32(xhci, xhci->db_base, 0);
    
    return idx;
}

static int xhci_wait_for_event(struct xhci_host *xhci, uint32_t trb_type, uint64_t *param, uint32_t *status, uint32_t *control) {
    int timeout = 1000000;
    while(timeout--) {
        uint32_t idx = xhci->event_ring_dequeue_idx;
        struct xhci_trb *ev = &xhci->event_ring[idx];
        
        if ((ev->control & TRB_C) == (xhci->event_ring_cycle ? TRB_C : 0)) {
             
            if (param) *param = ev->param;
            if (status) *status = ev->status;
            if (control) *control = ev->control;
            
            uint32_t type = (ev->control >> 10) & 0x3F;
            
             
            xhci->event_ring_dequeue_idx++;
            if (xhci->event_ring_dequeue_idx >= 4096 / sizeof(struct xhci_trb)) {
                xhci->event_ring_dequeue_idx = 0;
                xhci->event_ring_cycle = !xhci->event_ring_cycle;
            }
            
             
            uint64_t erdp = xhci->event_ring_phys + xhci->event_ring_dequeue_idx * sizeof(struct xhci_trb);
            xhci_write64(xhci, xhci->rt_base + XHCI_ERDP(0), erdp | (1<<3));  
            
            if (type == trb_type) return 0;
        }
    }
    return -1;
}

static int xhci_enable_slot(struct xhci_host *xhci) {
    kprint_str("XHCI: Enabling Slot...\n");
    xhci_send_command(xhci, 0, 0, 0, TRB_ENABLE_SLOT);
    
    uint64_t param;
    uint32_t status, control;
    if (xhci_wait_for_event(xhci, TRB_COMMAND_COMPLETION, &param, &status, &control) == 0) {
        uint32_t cc = (status >> 24) & 0xFF;
        if (cc == 1) {  
             uint32_t slot_id = (control >> 24) & 0xFF;
             kprint_str("XHCI: Slot Enabled, ID: ");
             kprint_dec(slot_id);
             kprint_newline();
             return slot_id;
        } else {
             kprint_str("XHCI: Enable Slot Failed, CC=");
             kprint_dec(cc);
             kprint_newline();
        }
    } else {
        kprint_str("XHCI: Enable Slot Timeout\n");
    }
    return -1;
}

static void xhci_port_check(struct xhci_host *xhci) {
    for (uint32_t i = 0; i < xhci->max_ports; i++) {
        uint32_t portsc = xhci_read32(xhci, xhci->op_base + XHCI_PORTSC(i));
        
        if (portsc & XHCI_PS_CCS) {
            kprint_str("XHCI: Port ");
            kprint_dec(i + 1);
            kprint_str(" Connected\n");
            
             
            if (!(portsc & XHCI_PS_PED)) {
                xhci_write32(xhci, xhci->op_base + XHCI_PORTSC(i), portsc | XHCI_PS_PR);
                xhci_enable_slot(xhci);
            }
        }
    }
}

static int xhci_probe(struct device *dev) {
    struct pci_device *pdev = to_pci_device(dev);
    
    if (pdev->class_code != XHCI_PCI_CLASS || 
        pdev->subclass != XHCI_PCI_SUBCLASS || 
        pdev->prog_if != XHCI_PCI_PROG_IF) {
        return -1;
    }
    
    kprint_str("XHCI: Controller Found\n");
    pci_enable_device(pdev);
    
    xhci_host = (struct xhci_host*)kmalloc(sizeof(struct xhci_host));
    memset(xhci_host, 0, sizeof(struct xhci_host));
    xhci_host->pdev = pdev;
    
     
    uint64_t mmio_phys = (uint64_t)pdev->bar[0] & ~0xF;
    xhci_host->mmio_base = (uint64_t)ioremap(mmio_phys, 0x10000);  
    
    xhci_host->cap_base = xhci_host->mmio_base;
    xhci_host->cap_len = *((uint8_t*)xhci_host->cap_base);
    xhci_host->op_base = xhci_host->cap_base + xhci_host->cap_len;
    xhci_host->rt_base = xhci_host->op_base + XHCI_RTSOFF;
    xhci_host->db_base = xhci_host->op_base + XHCI_DBOFF;  
    
    uint32_t hcsparams1 = xhci_read32(xhci_host, xhci_host->cap_base + XHCI_HCSPARAMS1);
    xhci_host->max_slots = hcsparams1 & 0xFF;
    xhci_host->max_ports = (hcsparams1 >> 24) & 0xFF;
    
    kprint_str("XHCI: Max Slots: "); kprint_dec(xhci_host->max_slots); kprint_newline();
    kprint_str("XHCI: Max Ports: "); kprint_dec(xhci_host->max_ports); kprint_newline();
    
    xhci_reset(xhci_host);
    
     
    uint32_t config = xhci_read32(xhci_host, xhci_host->op_base + XHCI_CONFIG);
    config = (config & ~0xFF) | xhci_host->max_slots;
    xhci_write32(xhci_host, xhci_host->op_base + XHCI_CONFIG, config);
    
    xhci_init_memory(xhci_host);
    
     
    uint32_t cmd = xhci_read32(xhci_host, xhci_host->op_base + XHCI_USBCMD);
    xhci_write32(xhci_host, xhci_host->op_base + XHCI_USBCMD, cmd | XHCI_CMD_RS);
    
    xhci_port_check(xhci_host);
    
    return 0;
}

static struct pci_driver xhci_driver = {
    .vendor_id = 0xFFFF,  
    .driver = {
        .name = "xhci",
        .probe = xhci_probe,
    }
};

void xhci_init(void) {
    pci_register_driver(&xhci_driver);
}

void xhci_print_info(void) {
    if (!xhci_host) {
        kprint_str("XHCI: Not initialized or no controller found.\n");
        return;
    }
    kprint_str("XHCI Host Controller:\n");
    kprint_str("  Max Slots: "); kprint_dec(xhci_host->max_slots); kprint_newline();
    kprint_str("  Max Ports: "); kprint_dec(xhci_host->max_ports); kprint_newline();
    kprint_str("  MMIO Base: "); kprint_hex(xhci_host->mmio_base); kprint_newline();
    
     
    for (uint32_t i = 0; i < xhci_host->max_ports; i++) {
        uint32_t portsc = xhci_read32(xhci_host, xhci_host->op_base + XHCI_PORTSC(i));
        if (portsc & XHCI_PS_CCS) {
            kprint_str("  Port "); kprint_dec(i+1); kprint_str(": Connected\n");
        }
    }
}
