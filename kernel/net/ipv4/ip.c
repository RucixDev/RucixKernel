#include "net/ip.h"
#include "net/ethernet.h"
#include "console.h"
#include "string.h"
#include "net/byteorder.h"

#include "net/icmp.h"
#include "net/udp.h"
#include "net/tcp.h"

uint16_t ip_checksum(void *vdata, size_t length) {
    uint16_t *data = (uint16_t *)vdata;
    uint32_t acc = 0;
    for (size_t i = 0; i < length / 2; i++) {
        acc += data[i];
    }
    if (length & 1) {
        acc += ((uint8_t *)vdata)[length - 1];
    }
    while (acc >> 16) {
        acc = (acc & 0xFFFF) + (acc >> 16);
    }
    return ~acc;
}

int ip_send_packet(struct sk_buff *skb, uint32_t dest_ip, uint8_t protocol) {
    struct net_device *dev = skb->dev;
    if (!dev) {
        kfree_skb(skb);
        return -1;
    }
    
    struct iphdr *iph = (struct iphdr *)skb_push(skb, sizeof(struct iphdr));
    memset(iph, 0, sizeof(struct iphdr));
    
    iph->version = 4;
    iph->ihl = 5;
    iph->tos = 0;
    iph->tot_len = htons(skb->len);
    iph->id = htons(1); 
    iph->frag_off = 0;
    iph->ttl = 64;
    iph->protocol = protocol;
    iph->check = 0;
    iph->saddr = dev->ip_addr;
    iph->daddr = dest_ip;
    
    iph->check = ip_checksum(iph, sizeof(struct iphdr));
    
    struct ethhdr *eth = (struct ethhdr *)skb_push(skb, sizeof(struct ethhdr));
    eth->h_proto = htons(ETH_P_IP);
    memcpy(eth->h_source, dev->dev_addr, 6);
    
    if (dest_ip == 0xFFFFFFFF) {
        memset(eth->h_dest, 0xFF, 6);
    } else {
        memset(eth->h_dest, 0xFF, 6);
    }
    
    extern int dev_queue_xmit(struct sk_buff *skb);
    return dev_queue_xmit(skb);
}

 
int ip_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt) {
    (void)dev;
    (void)pt;
    struct iphdr *iph = (struct iphdr *)skb->data;

    if (skb->len < sizeof(struct iphdr)) { 
        goto drop;
    }
    
    if (iph->version != 4) { 
        goto drop;
    }
    
    skb->network_header = skb->data;
    
    if (iph->protocol == IPPROTO_ICMP) {
        icmp_rcv(skb);
        return 0;
    }
    
    if (iph->protocol == IPPROTO_UDP) {
        udp_rcv(skb, dev, pt);
        return 0;
    }
    
    if (iph->protocol == IPPROTO_TCP) {
        tcp_rcv(skb, dev, pt);
        return 0;
    }

    kfree_skb(skb);
    return 0;

drop:
    kfree_skb(skb);
    return 0;
}

static struct packet_type ip_packet_type = {
    .type = 0x0008,  
    .func = ip_rcv,
};

void ip_init() {
    dev_add_pack(&ip_packet_type);
    kprint_str("IP: Initialized\n");
}
