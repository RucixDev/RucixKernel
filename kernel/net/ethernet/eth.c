#include "net/ethernet.h"
#include "net/netdevice.h"
#include "net/skbuff.h"
#include "console.h"
#include "string.h"

 
uint16_t eth_type_trans(struct sk_buff *skb, struct net_device *dev) {
    struct ethhdr *eth;
    
    skb->dev = dev;
    skb->mac_header = skb->data;

    eth = (struct ethhdr *)skb_pull(skb, ETH_HLEN);

    if (eth->h_dest[0] & 1) {
        if (memcmp(eth->h_dest, dev->broadcast, ETH_ALEN) == 0)
            skb->packet_type = 1;  
        else
            skb->packet_type = 2;  
    } else if (memcmp(eth->h_dest, dev->dev_addr, ETH_ALEN) != 0) {
        skb->packet_type = 3;  
    }

    return eth->h_proto;
}
