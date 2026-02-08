#ifndef _SOUND_CORE_H
#define _SOUND_CORE_H

#include "types.h"
#include "list.h"

 
#define SND_FMT_S16_LE  0
#define SND_FMT_S32_LE  1
#define SND_FMT_U8      2

 
struct snd_card {
    int number;
    char id[16];
    char driver[16];
    char shortname[32];
    char longname[80];
    
    struct list_head devices;
    struct list_head list;
    
    void *private_data;
};

 
#define SNDRV_PCM_STREAM_PLAYBACK 0
#define SNDRV_PCM_STREAM_CAPTURE  1

struct snd_pcm_substream;

struct snd_pcm_ops {
    int (*open)(struct snd_pcm_substream *substream);
    int (*close)(struct snd_pcm_substream *substream);
    int (*prepare)(struct snd_pcm_substream *substream);
    int (*trigger)(struct snd_pcm_substream *substream, int cmd);
    uint64_t (*pointer)(struct snd_pcm_substream *substream);
};

struct snd_pcm_runtime {
    uint32_t rate;
    uint32_t channels;
    uint32_t format;
    uint32_t period_size;
    uint32_t buffer_size;
    
    uint64_t dma_addr;
    void *dma_area;
    uint64_t dma_bytes;
};

struct snd_pcm_substream {
    struct snd_pcm *pcm;
    struct snd_pcm_runtime *runtime;
    const struct snd_pcm_ops *ops;
    int stream;
    void *private_data;
};

struct snd_pcm {
    struct snd_card *card;
    struct list_head list;
    int device;
    char name[80];
    
    struct snd_pcm_substream streams[2];  
};

 
int snd_card_new(int index, const char *id, struct snd_card **card_ret);
int snd_card_register(struct snd_card *card);
int snd_pcm_new(struct snd_card *card, const char *id, int device, int playback_count, int capture_count, struct snd_pcm **pcm_ret);

#endif
