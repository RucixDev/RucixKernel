#include "process.h"
#include "waitqueue.h"
#include "types.h"
#include "heap.h"
#include "string.h"
#include "console.h"

#define MSG_MAX 256
#define MSG_SIZE 1024

typedef struct msg_entry {
    long type;
    char data[MSG_SIZE];
    int size;
    struct msg_entry* next;
} msg_entry_t;

typedef struct msg_queue {
    int id;
    int key;
    mutex_t lock;
    cond_t cond_recv;
    msg_entry_t* head;
    msg_entry_t* tail;
    int count;
    struct msg_queue* next;
} msg_queue_t;

static msg_queue_t* queues = 0;
static int next_id = 1;
static mutex_t queues_lock;

void msg_init() {
    mutex_init(&queues_lock);
}

msg_queue_t* find_queue(int id) {
    msg_queue_t* q = queues;
    while (q) {
        if (q->id == id) return q;
        q = q->next;
    }
    return 0;
}

msg_queue_t* find_queue_key(int key) {
    msg_queue_t* q = queues;
    while (q) {
        if (q->key == key) return q;
        q = q->next;
    }
    return 0;
}

int sys_msgget(int key, int flags) {
    (void)flags;
    mutex_lock(&queues_lock);
    
    msg_queue_t* q = find_queue_key(key);
    if (q) {
        mutex_unlock(&queues_lock);
        return q->id;
    }
    
     
    q = (msg_queue_t*)kmalloc(sizeof(msg_queue_t));
    if (!q) {
        mutex_unlock(&queues_lock);
        return -1;
    }
    
    q->id = next_id++;
    q->key = key;
    mutex_init(&q->lock);
    cond_init(&q->cond_recv);
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    
    q->next = queues;
    queues = q;
    
    mutex_unlock(&queues_lock);
    return q->id;
}

int sys_msgsnd(int msqid, const void* msgp, int msgsz, int msgflg) {
    (void)msgflg;
    if (msgsz > MSG_SIZE) return -1;

    mutex_lock(&queues_lock);
    msg_queue_t* q = find_queue(msqid);
    mutex_unlock(&queues_lock);
    
    if (!q) return -1;
    
    mutex_lock(&q->lock);
    
    msg_entry_t* entry = (msg_entry_t*)kmalloc(sizeof(msg_entry_t));
    if (!entry) {
        mutex_unlock(&q->lock);
        return -1;
    }
    
    long type = *(long*)msgp;
    char* text = (char*)msgp + sizeof(long);
    
    entry->type = type;
    entry->size = msgsz;
    memcpy(entry->data, text, msgsz);
    entry->next = 0;
    
    if (q->tail) {
        q->tail->next = entry;
        q->tail = entry;
    } else {
        q->head = entry;
        q->tail = entry;
    }
    q->count++;
    
    cond_broadcast(&q->cond_recv);
    mutex_unlock(&q->lock);
    
    return 0;
}

int sys_msgrcv(int msqid, void* msgp, int msgsz, long msgtyp, int msgflg) {
    (void)msgflg;
    mutex_lock(&queues_lock);
    msg_queue_t* q = find_queue(msqid);
    mutex_unlock(&queues_lock);
    
    if (!q) return -1;
    
    mutex_lock(&q->lock);
    
    while (1) {
        msg_entry_t* prev = 0;
        msg_entry_t* curr = q->head;
        msg_entry_t* match = 0;
        
        while (curr) {
            if (msgtyp == 0 || curr->type == msgtyp) {
                match = curr;
                break;
            }
            prev = curr;
            curr = curr->next;
        }
        
        if (match) {
             
            if (prev) {
                prev->next = match->next;
                if (match == q->tail) q->tail = prev;
            } else {
                q->head = match->next;
                if (!q->head) q->tail = 0;
            }
            q->count--;
            
            int copy_len = (match->size > msgsz) ? msgsz : match->size;
            *(long*)msgp = match->type;
            memcpy((char*)msgp + sizeof(long), match->data, copy_len);
            
            kfree(match);
            mutex_unlock(&q->lock);
            return copy_len;
        }
        
         
        cond_wait(&q->cond_recv, &q->lock);
    }
}
