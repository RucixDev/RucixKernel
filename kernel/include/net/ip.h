#ifndef _NET_IP_H
#define _NET_IP_H

#include <stdint.h>
#include "net/skbuff.h"
#include "net/netdevice.h"

struct iphdr {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t ihl:4;
    uint8_t version:4;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint8_t version:4;
    uint8_t ihl:4;
#else
#error "Unknown byte order"
#endif
    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
     
} __attribute__((packed));

#define IPPROTO_ICMP 1
#define IPPROTO_TCP  6
#define IPPROTO_UDP  17

void ip_init(void);
int ip_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt);
 

#endif  
