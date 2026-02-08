#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include "driver.h"

enum dma_data_direction {
    DMA_BIDIRECTIONAL = 0,
    DMA_TO_DEVICE = 1,
    DMA_FROM_DEVICE = 2,
    DMA_NONE = 3,
};

typedef uint64_t dma_addr_t;

void* dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle);
void dma_free_coherent(struct device *dev, size_t size, void *vaddr, dma_addr_t dma_handle);

dma_addr_t dma_map_single(struct device *dev, void *ptr, size_t size, enum dma_data_direction dir);
void dma_unmap_single(struct device *dev, dma_addr_t addr, size_t size, enum dma_data_direction dir);

#endif
