#ifndef _NET_NETDEVICE_H
#define _NET_NETDEVICE_H

#include <stdint.h>
#include "driver.h"
#include "net/skbuff.h"
#include "net/ethernet.h"

 
#define IFF_UP          0x1
#define IFF_BROADCAST   0x2
#define IFF_DEBUG       0x4
#define IFF_LOOPBACK    0x8
#define IFF_POINTOPOINT 0x10
#define IFF_NOTRAILERS  0x20
#define IFF_RUNNING     0x40
#define IFF_NOARP       0x80
#define IFF_PROMISC     0x100
#define IFF_ALLMULTI    0x200
#define IFF_MULTICAST   0x1000

struct net_device_stats {
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
};

struct net_device_ops {
    int (*init)(struct net_device *dev);
    void (*uninit)(struct net_device *dev);
    int (*open)(struct net_device *dev);
    int (*stop)(struct net_device *dev);
    int (*start_xmit)(struct sk_buff *skb, struct net_device *dev);
    int (*do_ioctl)(struct net_device *dev, void *cmd, int cmd_len);
};

struct net_device {
    char name[16];
    
     
    uint8_t dev_addr[ETH_ALEN];  
    uint8_t broadcast[ETH_ALEN];
    
    uint16_t mtu;
    uint16_t type;               
    uint16_t hard_header_len;    
    
    uint32_t ip_addr;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    
    uint32_t flags;
    
    struct device dev;           
    
    const struct net_device_ops *netdev_ops;
    
    struct net_device_stats stats;
    void *priv;
    
    struct list_head node;
};

 
extern struct list_head net_devices;

void net_dev_init(void);

struct net_device *alloc_netdev(int sizeof_priv, const char *name, void (*setup)(struct net_device *));
int register_netdev(struct net_device *dev);
void unregister_netdev(struct net_device *dev);
void free_netdev(struct net_device *dev);
struct net_device *dev_get_by_name(const char *name);

struct packet_type {
    uint16_t type;   
    struct net_device *dev;  
    int (*func)(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt);
    struct list_head list;
};

void dev_add_pack(struct packet_type *pt);
void dev_remove_pack(struct packet_type *pt);

uint16_t eth_type_trans(struct sk_buff *skb, struct net_device *dev);

int netif_rx(struct sk_buff *skb);

#endif  
