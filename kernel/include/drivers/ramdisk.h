#ifndef RAMDISK_H
#define RAMDISK_H

#include "drivers/blockdev.h"

void ramdisk_init(void);
struct gendisk *create_ramdisk(int minor, int size);

#endif
