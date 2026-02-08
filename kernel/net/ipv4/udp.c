#include "net/udp.h"
#include "net/ip.h"
#include "console.h"
#include "string.h"
#include "net/byteorder.h"
#include "net/ethernet.h"
#include "net/dhcp.h"

int udp_send_packet(struct net_device *dev, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, void *data, uint32_t len) {
    struct sk_buff *skb = alloc_skb(len + sizeof(struct udphdr) + sizeof(struct iphdr) + sizeof(struct ethhdr) + 32);
    if (!skb) return -1;
    
    skb->dev = dev;
     
    skb_reserve(skb, sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr));
    
    uint8_t *payload = skb_put(skb, len);
    memcpy(payload, data, len);
    
    struct udphdr *uh = (struct udphdr *)skb_push(skb, sizeof(struct udphdr));
    uh->source = htons(src_port);
    uh->dest = htons(dest_port);
    uh->len = htons(len + sizeof(struct udphdr));
    uh->check = 0;  
    
    extern int ip_send_packet(struct sk_buff *skb, uint32_t dest_ip, uint8_t protocol);
    return ip_send_packet(skb, dest_ip, IPPROTO_UDP);
}

 
int udp_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt) {
    (void)dev;
    (void)pt;
    struct udphdr *uh = (struct udphdr *)(skb->data + sizeof(struct iphdr));
    (void)uh;
    
    if (skb->len < sizeof(struct iphdr) + sizeof(struct udphdr)) {
        goto drop;
    }
    
    uint16_t dest_port = ntohs(uh->dest);
    
    if (dest_port == 68) {
        dhcp_input(skb);
        return 0;  
    }

    kfree_skb(skb);
    return 0;

drop:
    kfree_skb(skb);
    return 0;
}

void udp_init() {
    kprint_str("UDP: Initialized\n");
}
