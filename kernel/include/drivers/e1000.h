#ifndef _DRIVERS_E1000_H
#define _DRIVERS_E1000_H

#include <stdint.h>
#include "drivers/pci.h"
#include "net/netdevice.h"

#define INTEL_VEND     0x8086   
#define E1000_DEV      0x100E   
#define E1000_82540EM  0x1000   
#define E1000_82545EM  0x100F   
#define E1000_I217     0x153A   
#define E1000_82577LM  0x10EA   

#define E1000_CTRL     0x00000   
#define E1000_STATUS   0x00008   
#define E1000_EERD     0x00014   
#define E1000_ICR      0x000C0   
#define E1000_ITR      0x000C4   
#define E1000_ICS      0x000C8   
#define E1000_IMS      0x000D0   
#define E1000_IMC      0x000D8   
#define E1000_RCTL     0x00100   
#define E1000_TCTL     0x00400   
#define E1000_TIPG     0x00410   
#define E1000_RDBAL    0x02800   
#define E1000_RDBAH    0x02804   
#define E1000_RDLEN    0x02808   
#define E1000_RDH      0x02810   
#define E1000_RDT      0x02818   
#define E1000_TDBAL    0x03800   
#define E1000_TDBAH    0x03804   
#define E1000_TDLEN    0x03808   
#define E1000_TDH      0x03810   
#define E1000_TDT      0x03818   
#define E1000_MTA      0x05200   

#define E1000_CTRL_SLU        0x00000040     
#define E1000_CTRL_RST        0x04000000     

#define E1000_STATUS_LU       0x00000002     

#define E1000_RCTL_EN         0x00000002     
#define E1000_RCTL_SBP        0x00000004     
#define E1000_RCTL_UPE        0x00000008     
#define E1000_RCTL_MPE        0x00000010     
#define E1000_RCTL_LPE        0x00000020     
#define E1000_RCTL_BAM        0x00008000     

#define E1000_TCTL_EN         0x00000002     
#define E1000_TCTL_PSP        0x00000008     
#define E1000_TCTL_CT         0x000000F0     
#define E1000_TCTL_COLD       0x003FF000     
#define E1000_TCTL_RTLC       0x01000000     

#define E1000_ICR_TXDW        0x00000001     
#define E1000_ICR_TXQE        0x00000002     
#define E1000_ICR_LSC         0x00000004     
#define E1000_ICR_RXSEQ       0x00000008     
#define E1000_ICR_RXDMT0      0x00000010     
#define E1000_ICR_RXO         0x00000040     
#define E1000_ICR_RXT0        0x00000080     

#define E1000_CMD_EOP         (1 << 0)       
#define E1000_CMD_IFCS        (1 << 1)       
#define E1000_CMD_IC          (1 << 2)       
#define E1000_CMD_RS          (1 << 3)       
#define E1000_CMD_RPS         (1 << 4)       
#define E1000_CMD_VLE         (1 << 6)       
#define E1000_CMD_IDE         (1 << 7)       

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 8

struct e1000_rx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t status;
    volatile uint8_t errors;
    volatile uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t cso;
    volatile uint8_t cmd;
    volatile uint8_t status;
    volatile uint8_t css;
    volatile uint16_t special;
} __attribute__((packed));

void e1000_init_driver();

#endif
