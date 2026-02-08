#include "net/netdevice.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "spinlock.h"

 
LIST_HEAD(net_devices);
static spinlock_t net_dev_lock;

 
static LIST_HEAD(ptype_base);
static spinlock_t ptype_lock;

void net_dev_init() {
     
    INIT_LIST_HEAD(&net_devices);
     
    INIT_LIST_HEAD(&ptype_base);
    
    spinlock_init(&net_dev_lock);
    spinlock_init(&ptype_lock);
    
    kprint_str("net_dev_init: &net_devices = ");
    kprint_hex((uint64_t)&net_devices);
    kprint_newline();
    kprint_str("net_dev_init: net_devices.next = ");
    kprint_hex((uint64_t)net_devices.next);
    kprint_newline();
    
    kprint_str("Net Device Core Initialized.\n");
}

void dev_add_pack(struct packet_type *pt) {
    spinlock_acquire(&ptype_lock);
    list_add_tail(&pt->list, &ptype_base);
    spinlock_release(&ptype_lock);
}

void dev_remove_pack(struct packet_type *pt) {
    spinlock_acquire(&ptype_lock);
    list_del(&pt->list);
    spinlock_release(&ptype_lock);
}

struct net_device *alloc_netdev(int sizeof_priv, const char *name, void (*setup)(struct net_device *)) {
    int alloc_size = sizeof(struct net_device) + sizeof_priv;
    struct net_device *dev = (struct net_device *)kmalloc(alloc_size);
    if (!dev) return 0;
    
    kprint_str("alloc_netdev: dev="); kprint_hex((uint64_t)dev); kprint_newline();
    
    memset(dev, 0, alloc_size);
    
    if (sizeof_priv) {
        dev->priv = (void *)(dev + 1);
    }
    
    if (name) {
        strncpy(dev->name, name, 15);
    }
    
    if (setup) {
        setup(dev);
    }
    
    return dev;
}

int register_netdev(struct net_device *dev) {
    if (!dev) return -1;
    
    kprint_str("register_netdev: Acquiring lock...\n");
    spinlock_acquire(&net_dev_lock);
    kprint_str("register_netdev: Lock acquired.\n");
    
     
    struct list_head *iter;
    kprint_str("register_netdev: Checking collisions. list_head=");
    kprint_hex((uint64_t)&net_devices);
    kprint_str(" next=");
    kprint_hex((uint64_t)net_devices.next);
    kprint_newline();
    
    list_for_each(iter, &net_devices) {
        kprint_str("register_netdev: iter=");
        kprint_hex((uint64_t)iter);
        kprint_newline();
        
        struct net_device *d = list_entry(iter, struct net_device, node);
        if (strcmp(d->name, dev->name) == 0) {
            spinlock_release(&net_dev_lock);
            kprint_str("register_netdev: Name collision ");
            kprint_str(dev->name);
            kprint_newline();
            return -1;
        }
    }
    
    kprint_str("register_netdev: Initializing ops...\n");
     
    if (dev->netdev_ops && dev->netdev_ops->init) {
        if (dev->netdev_ops->init(dev) < 0) {
            spinlock_release(&net_dev_lock);
            return -1;
        }
    }
    
    kprint_str("register_netdev: Adding to list...\n");
    list_add_tail(&dev->node, &net_devices);
    
    spinlock_release(&net_dev_lock);
    
    kprint_str("Network Device Registered: ");
    kprint_str(dev->name);
    kprint_newline();
    
    return 0;
}

void unregister_netdev(struct net_device *dev) {
    if (!dev) return;
    
    spinlock_acquire(&net_dev_lock);
    list_del(&dev->node);
    spinlock_release(&net_dev_lock);
    
    if (dev->netdev_ops && dev->netdev_ops->uninit) {
        dev->netdev_ops->uninit(dev);
    }
}

void free_netdev(struct net_device *dev) {
    kfree(dev);
}

struct net_device *dev_get_by_name(const char *name) {
    struct list_head *iter;
    spinlock_acquire(&net_dev_lock);
    list_for_each(iter, &net_devices) {
        struct net_device *dev = list_entry(iter, struct net_device, node);
        if (strcmp(dev->name, name) == 0) {
            spinlock_release(&net_dev_lock);
            return dev;
        }
    }
    spinlock_release(&net_dev_lock);
    return 0;
}

 
int netif_rx(struct sk_buff *skb) {
    if (!skb) return -1;
    
     
    if (skb->dev) {
        skb->dev->stats.rx_packets++;
        skb->dev->stats.rx_bytes += skb->len;
    }

    spinlock_acquire(&ptype_lock);
    struct list_head *iter;
    int handled = 0;
    list_for_each(iter, &ptype_base) {
        struct packet_type *pt = list_entry(iter, struct packet_type, list);
        if (pt->type == skb->protocol || pt->type == 0xFFFF) {  

             pt->func(skb, skb->dev, pt);
             handled = 1;
             break; 
        }
    }
    spinlock_release(&ptype_lock);
    
    if (!handled) {
         
        kfree_skb(skb);
    }
    
    return 0;
}

int dev_queue_xmit(struct sk_buff *skb) {
    if (!skb) return -1;
    
    struct net_device *dev = skb->dev;
    if (!dev) {
        kfree_skb(skb);
        return -1;
    }
    
    if (dev->netdev_ops && dev->netdev_ops->start_xmit) {
        dev->stats.tx_packets++;
        dev->stats.tx_bytes += skb->len;
        return dev->netdev_ops->start_xmit(skb, dev);
    }
    
    kfree_skb(skb);
    return -1;
}
