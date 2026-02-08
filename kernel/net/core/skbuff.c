#include "net/skbuff.h"
#include "heap.h"
#include "string.h"
#include "console.h"

struct sk_buff *alloc_skb(uint32_t size) {
    struct sk_buff *skb = (struct sk_buff *)kmalloc(sizeof(struct sk_buff));
    if (!skb) return 0;
    
    memset(skb, 0, sizeof(struct sk_buff));
    
     
     
    skb->head = (uint8_t *)kmalloc(size);
    if (!skb->head) {
        kfree(skb);
        return 0;
    }
    
    skb->data = skb->head;
    skb->tail = skb->head;
    skb->end = skb->head + size;
    skb->true_size = size;
    skb->len = 0;
    
    return skb;
}

void kfree_skb(struct sk_buff *skb) {
    if (!skb) return;
    
    if (skb->head) {
        kfree(skb->head);
    }
    kfree(skb);
}

uint8_t *skb_put(struct sk_buff *skb, uint32_t len) {
    uint8_t *tmp = skb->tail;
    skb->tail += len;
    skb->len += len;
    
    if (skb->tail > skb->end) {
        kprint_str("skb_put: buffer overflow!\n");
         
        return 0; 
    }
    
    return tmp;
}

uint8_t *skb_push(struct sk_buff *skb, uint32_t len) {
    skb->data -= len;
    skb->len += len;
    
    if (skb->data < skb->head) {
        kprint_str("skb_push: buffer underflow!\n");
        return 0;
    }
    
    return skb->data;
}

uint8_t *skb_pull(struct sk_buff *skb, uint32_t len) {
    if (len > skb->len) {
        return 0;
    }
    
    uint8_t *tmp = skb->data;
    skb->data += len;
    skb->len -= len;
    
    return tmp;
}

void skb_reserve(struct sk_buff *skb, uint32_t len) {
    skb->data += len;
    skb->tail += len;
    
    if (skb->tail > skb->end) {
        kprint_str("skb_reserve: buffer overflow!\n");
    }
}
