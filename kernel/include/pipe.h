#ifndef PIPE_H
#define PIPE_H

#include "vfs.h"
#include "waitqueue.h"
#include "spinlock.h"

#define PIPE_SIZE 4096

typedef struct {
    char buffer[PIPE_SIZE];
    int read_pos;
    int write_pos;
    int bytes_available;
    int readers;
    int writers;
    wait_queue_t read_wait;
    wait_queue_t write_wait;
    spinlock_t lock;
} pipe_t;

int pipe_create(int fds[2]);

#endif
