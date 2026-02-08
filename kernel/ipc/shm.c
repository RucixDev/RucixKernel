#include "types.h"
#include "heap.h"
#include "process.h"
#include "waitqueue.h"
#include "string.h"

 

#define SHM_MAX 64

typedef struct shm_segment {
    int id;
    int key;
    uint64_t size;
    void* phys_addr;  
    int ref_count;
    struct shm_segment* next;
} shm_segment_t;

static shm_segment_t* segments = 0;
static int next_shm_id = 1;
static mutex_t shm_lock;

void shm_init() {
    mutex_init(&shm_lock);
}

shm_segment_t* find_shm(int id) {
    shm_segment_t* s = segments;
    while (s) {
        if (s->id == id) return s;
        s = s->next;
    }
    return 0;
}

shm_segment_t* find_shm_key(int key) {
    shm_segment_t* s = segments;
    while (s) {
        if (s->key == key) return s;
        s = s->next;
    }
    return 0;
}

int sys_shmget(int key, uint64_t size, int flags) {
    (void)flags;
    mutex_lock(&shm_lock);
    
    shm_segment_t* s = find_shm_key(key);
    if (s) {
        mutex_unlock(&shm_lock);
        return s->id;
    }
    
     
    s = (shm_segment_t*)kmalloc(sizeof(shm_segment_t));
    if (!s) {
        mutex_unlock(&shm_lock);
        return -1;
    }
    
     
     
    s->phys_addr = kmalloc(size);
    if (!s->phys_addr) {
        kfree(s);
        mutex_unlock(&shm_lock);
        return -1;
    }
    memset(s->phys_addr, 0, size);
    
    s->id = next_shm_id++;
    s->key = key;
    s->size = size;
    s->ref_count = 0;
    
    s->next = segments;
    segments = s;
    
    mutex_unlock(&shm_lock);
    return s->id;
}

void* sys_shmat(int shmid, const void* shmaddr, int shmflg) {
    (void)shmaddr;
    (void)shmflg;
    mutex_lock(&shm_lock);
    shm_segment_t* s = find_shm(shmid);
    if (!s) {
        mutex_unlock(&shm_lock);
        return (void*)-1;
    }
    
    s->ref_count++;
    mutex_unlock(&shm_lock);
    
     
     
    return s->phys_addr;
}

int sys_shmdt(const void* shmaddr) {
     
    mutex_lock(&shm_lock);
    shm_segment_t* s = segments;
    while (s) {
        if (s->phys_addr == shmaddr) {
            s->ref_count--;
             
            mutex_unlock(&shm_lock);
            return 0;
        }
        s = s->next;
    }
    mutex_unlock(&shm_lock);
    return -1;
}
