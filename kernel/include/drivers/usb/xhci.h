#ifndef _DRIVERS_USB_XHCI_H
#define _DRIVERS_USB_XHCI_H

#include "types.h"
#include "drivers/pci.h"
#include "drivers/usb/usb.h"
#include "spinlock.h"

#define XHCI_PCI_CLASS 0x0C
#define XHCI_PCI_SUBCLASS 0x03
#define XHCI_PCI_PROG_IF 0x30

#define XHCI_CAPLENGTH      0x00
#define XHCI_HCIVERSION     0x02
#define XHCI_HCSPARAMS1     0x04
#define XHCI_HCSPARAMS2     0x08
#define XHCI_HCSPARAMS3     0x0C
#define XHCI_HCCPARAMS1     0x10
#define XHCI_DBOFF          0x14
#define XHCI_RTSOFF         0x18
#define XHCI_HCCPARAMS2     0x1C

#define XHCI_USBCMD         0x00
#define XHCI_USBSTS         0x04
#define XHCI_PAGESIZE       0x08
#define XHCI_DNCTRL         0x14
#define XHCI_CRCR           0x18
#define XHCI_DCBAAP         0x30
#define XHCI_CONFIG         0x38

#define XHCI_MFINDEX        0x00
#define XHCI_IMAN(n)        (0x20 + (n) * 32)
#define XHCI_IMOD(n)        (0x24 + (n) * 32)
#define XHCI_ERSTSZ(n)      (0x28 + (n) * 32)
#define XHCI_ERSTBA(n)      (0x30 + (n) * 32)
#define XHCI_ERDP(n)        (0x38 + (n) * 32)

#define XHCI_DB(n)          ((n) * 4)

#define XHCI_CMD_RS         (1 << 0)   
#define XHCI_CMD_HCRST      (1 << 1)   
#define XHCI_CMD_INTE       (1 << 2)   
#define XHCI_CMD_HSEE       (1 << 3)   
#define XHCI_CMD_LHCRST     (1 << 7)   

#define XHCI_STS_HCH        (1 << 0)   
#define XHCI_STS_HSE        (1 << 2)   
#define XHCI_STS_EINT       (1 << 3)   
#define XHCI_STS_PCD        (1 << 4)   
#define XHCI_STS_SSS        (1 << 8)   
#define XHCI_STS_RSS        (1 << 9)   
#define XHCI_STS_SRE        (1 << 10)  
#define XHCI_STS_CNR        (1 << 11)  
#define XHCI_STS_HCE        (1 << 12)  

#define XHCI_PORTSC(n)      (0x400 + (n) * 16)
#define XHCI_PS_CCS         (1 << 0)   
#define XHCI_PS_PED         (1 << 1)   
#define XHCI_PS_OCA         (1 << 3)   
#define XHCI_PS_PR          (1 << 4)   
#define XHCI_PS_PLS         (0xF << 5)  
#define XHCI_PS_PP          (1 << 9)   
#define XHCI_PS_CSC         (1 << 17)  
#define XHCI_PS_PEC         (1 << 18)  
#define XHCI_PS_WRC         (1 << 19)  
#define XHCI_PS_OCC         (1 << 20)  
#define XHCI_PS_PRC         (1 << 21)  

#define TRB_NORMAL          1
#define TRB_SETUP_STAGE     2
#define TRB_DATA_STAGE      3
#define TRB_STATUS_STAGE    4
#define TRB_ISOCH           5
#define TRB_LINK            6
#define TRB_EVENT_DATA      7
#define TRB_NOOP            8
#define TRB_ENABLE_SLOT     9
#define TRB_DISABLE_SLOT    10
#define TRB_ADDRESS_DEVICE  11
#define TRB_CONFIGURE_EP    12
#define TRB_EVALUATE_CTX    13
#define TRB_RESET_EP        14
#define TRB_STOP_EP         15
#define TRB_SET_TR_DEQ      16
#define TRB_RESET_DEVICE    17
#define TRB_FORCE_EVENT     18
#define TRB_NEGOTIATE_BW    19
#define TRB_SET_LATENCY     20
#define TRB_GET_PORT_BW     21
#define TRB_FORCE_HEADER    22
#define TRB_NOOP_CMD        23
#define TRB_TRANSFER_EVENT  32
#define TRB_COMMAND_COMPLETION 33
#define TRB_PORT_STATUS_CHANGE 34
#define TRB_BANDWIDTH_REQUEST 35
#define TRB_DOORBELL_EVENT  36
#define TRB_HOST_CONTROLLER 37
#define TRB_DEVICE_NOTIFICATION 38
#define TRB_MFINDEX_WRAP    39

struct xhci_trb {
    uint64_t param;
    uint32_t status;
    uint32_t control;
} __attribute__((packed));

 
struct xhci_erst_entry {
    uint64_t base;
    uint32_t size;
    uint32_t rsvd;
} __attribute__((packed));

 
struct xhci_input_control_ctx {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t rsvd[6];
} __attribute__((packed));

 
struct xhci_slot_ctx {
    uint32_t field1;
    uint32_t field2;
    uint32_t field3;
    uint32_t field4;
    uint32_t rsvd[4];
} __attribute__((packed));

 
struct xhci_ep_ctx {
    uint32_t field1;
    uint32_t field2;
    uint64_t tr_dequeue;
    uint32_t field4;
    uint32_t rsvd[3];
} __attribute__((packed));

 
struct xhci_dev_ctx {
    struct xhci_slot_ctx slot;
    struct xhci_ep_ctx eps[31];
} __attribute__((packed));

 
struct xhci_host {
    struct pci_device *pdev;
    uint64_t mmio_base;
    uint64_t cap_base;
    uint64_t op_base;
    uint64_t rt_base;
    uint64_t db_base;
    
    uint8_t cap_len;
    uint32_t max_slots;
    uint32_t max_ports;
    
    struct xhci_trb *cmd_ring;
    uint64_t cmd_ring_phys;
    uint32_t cmd_ring_enqueue_idx;
    uint8_t cmd_ring_cycle;
    
    struct xhci_erst_entry *erst;
    uint64_t erst_phys;
    
    struct xhci_trb *event_ring;
    uint64_t event_ring_phys;
    uint32_t event_ring_dequeue_idx;
    uint8_t event_ring_cycle;
    
    uint64_t *dcbaa;
    uint64_t dcbaa_phys;
    
    spinlock_t lock;
};

 
#define TRB_C               (1 << 0)  
#define TRB_TC              (1 << 1)  
#define TRB_ISP             (1 << 2)  
#define TRB_IOC             (1 << 5)  
#define TRB_IDT             (1 << 6)  
#define TRB_TYPE(n)         ((n) << 10)

 
#define SLOT_CTX_ROUTE(n)   ((n) << 0)
#define SLOT_CTX_SPEED(n)   ((n) << 20)
#define SLOT_CTX_MTT        (1 << 25)
#define SLOT_CTX_HUB        (1 << 26)
#define SLOT_CTX_ENTRIES(n) ((n) << 27)

void xhci_init(void);

#endif
