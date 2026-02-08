#ifndef NVME_H
#define NVME_H

#include <stdint.h>
#include "drivers/pci.h"

 
#define NVME_REG_CAP    0x0000  
#define NVME_REG_VS     0x0008  
#define NVME_REG_INTMS  0x000C  
#define NVME_REG_INTMC  0x0010  
#define NVME_REG_CC     0x0014  
#define NVME_REG_CSTS   0x001C  
#define NVME_REG_NSSR   0x0020  
#define NVME_REG_AQA    0x0024  
#define NVME_REG_ASQ    0x0028  
#define NVME_REG_ACQ    0x0030  
#define NVME_REG_CMBLOC 0x0038  
#define NVME_REG_CMBSZ  0x003C  

#define NVME_REG_SQ0TDBL 0x1000  

#define NVME_CC_EN      (1 << 0)
#define NVME_CC_CSS_NVM (0 << 4)
#define NVME_CC_MPS_4K  (0 << 7)
#define NVME_CC_AMS_RR  (0 << 11)
#define NVME_CC_SHN_NONE (0 << 14)
#define NVME_CC_IOSQES  (6 << 16)  
#define NVME_CC_IOCQES  (4 << 20)  

#define NVME_CSTS_RDY   (1 << 0)
#define NVME_CSTS_CFS   (1 << 1)

#define NVME_CMD_FLUSH          0x00
#define NVME_CMD_WRITE          0x01
#define NVME_CMD_READ           0x02
#define NVME_CMD_IDENTIFY       0x06
#define NVME_CMD_SET_FEATURES   0x09
#define NVME_CMD_GET_FEATURES   0x0A

struct nvme_sqe {
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t command_id;
    uint32_t nsid;
    uint64_t rsvd2;
    uint64_t metadata;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
};

struct nvme_cqe {
    uint32_t cdw0;
    uint32_t rsvd1;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t command_id;
    uint16_t status;
};

struct nvme_id_ns {
    uint64_t nsze;  
    uint64_t ncap;  
    uint64_t nuse;  
    uint8_t  nsfeat;
    uint8_t  nlbaf;  
    uint8_t  flbas;  
    uint8_t  mc;
    uint8_t  dpc;
    uint8_t  dps;
    uint8_t  nmic;
    uint8_t  rescap;
    uint8_t  fpi;
    uint8_t  dlfeat;
    uint16_t nawun;
    uint16_t nawupf;
    uint16_t nacwu;
    uint16_t nabsn;
    uint16_t nabo;
    uint16_t nabspf;
    uint16_t noiob;
    uint8_t  nvmcap[16];
    uint8_t  rsvd64[40];
    uint8_t  nguid[16];
    uint64_t eui64;
    struct nvme_lbaf {
        uint16_t ms;
        uint8_t  ds;  
        uint8_t  rp;
    } lbaf[16];
    uint8_t  rsvd192[192];
    uint8_t  vs[3712];
};

void nvme_init();

#endif
