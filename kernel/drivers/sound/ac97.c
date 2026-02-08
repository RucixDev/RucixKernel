#include "sound/core.h"
#include "drivers/pci.h"
#include "io.h"
#include "heap.h"
#include "vmm.h"
#include "console.h"
#include "string.h"

#define AC97_VENDOR_ID 0x8086
#define AC97_DEVICE_ID 0x2415  

#define AC97_PO_BDBAR   0x10  
#define AC97_PO_LVI     0x15  
#define AC97_PO_SR      0x16  
#define AC97_PO_CR      0x1B  

struct ac97_bd {
    uint32_t ptr;
    uint32_t len_ctrl;
} __attribute__((packed));

struct ac97_chip {
    struct snd_card *card;
    struct pci_device *pdev;
    
    uint32_t nam_base;
    uint32_t nabm_base;
    
    struct ac97_bd *bdl;  
    uint64_t bdl_phys;
    
    void *dma_buffer;
    uint64_t dma_phys;
};

static int ac97_playback_open(struct snd_pcm_substream *substream) {
     
     
    struct snd_pcm_runtime *runtime = (struct snd_pcm_runtime*)kmalloc(sizeof(struct snd_pcm_runtime));
    memset(runtime, 0, sizeof(struct snd_pcm_runtime));
    substream->runtime = runtime;
    return 0;
}

static int ac97_playback_trigger(struct snd_pcm_substream *substream, int cmd) {
    struct ac97_chip *chip = (struct ac97_chip*)substream->pcm->card->private_data;
    
    if (cmd == 1) {  
        chip->bdl[0].ptr = chip->dma_phys;
        chip->bdl[0].len_ctrl = (0x1000 >> 1) | (1 << 31);  
        
        outl(chip->nabm_base + AC97_PO_BDBAR, chip->bdl_phys);
        outb(chip->nabm_base + AC97_PO_LVI, 0);  
        
        uint8_t cr = inb(chip->nabm_base + AC97_PO_CR);
        outb(chip->nabm_base + AC97_PO_CR, cr | 1);  
    } else {  
        uint8_t cr = inb(chip->nabm_base + AC97_PO_CR);
        outb(chip->nabm_base + AC97_PO_CR, cr & ~1);
    }
    return 0;
}

static struct snd_pcm_ops ac97_ops = {
    .open = ac97_playback_open,
    .trigger = ac97_playback_trigger,
};

static int ac97_probe(struct device *dev) {
    struct pci_device *pdev = to_pci_device(dev);
    
    if (pdev->vendor_id != AC97_VENDOR_ID || pdev->device_id != AC97_DEVICE_ID) {
        return -1;
    }
    
    pci_enable_device(pdev);
    
    struct snd_card *card;
    snd_card_new(0, "AC97", &card);
    
    struct ac97_chip *chip = (struct ac97_chip*)kmalloc(sizeof(struct ac97_chip));
    memset(chip, 0, sizeof(struct ac97_chip));
    chip->card = card;
    chip->pdev = pdev;
    chip->nam_base = pdev->bar[0] & ~1;  
    chip->nabm_base = pdev->bar[1] & ~1;
    
    card->private_data = chip;
    strcpy(card->driver, "AC97");
    strcpy(card->shortname, "Intel ICH5");
    strcpy(card->longname, "Intel AC97 Audio Controller");
    
    chip->bdl = (struct ac97_bd*)kmalloc(4096);
    memset(chip->bdl, 0, 4096);
    chip->bdl_phys = vmm_get_phys((uint64_t)chip->bdl);
    
    chip->dma_buffer = (void*)kmalloc(65536);
    memset(chip->dma_buffer, 0, 65536);
    chip->dma_phys = vmm_get_phys((uint64_t)chip->dma_buffer);
    
    struct snd_pcm *pcm;
    snd_pcm_new(card, "AC97 PCM", 0, 1, 0, &pcm);
    pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].ops = &ac97_ops;
    
    outw(chip->nam_base + 0, 0);  
    outl(chip->nabm_base + 0x2C, 2);  
    
    snd_card_register(card);
    
    return 0;
}

static struct pci_driver ac97_driver = {
    .vendor_id = AC97_VENDOR_ID,
    .device_id = AC97_DEVICE_ID,
    .driver = {
        .name = "ac97",
        .probe = ac97_probe,
    }
};

void ac97_init(void) {
    pci_register_driver(&ac97_driver);
}
