#ifndef CHARDEV_H
#define CHARDEV_H

#include "vfs.h"

#define MAX_CHAR_DEVICES 32

typedef struct {
    char name[32];
    int major;
    struct file_operations *fops;
} char_device_t;

void chardev_init(void);
int register_chrdev(int major, const char *name, struct file_operations *fops);
int unregister_chrdev(int major, const char *name);
struct file_operations* chardev_get_fops(int major);

#endif
