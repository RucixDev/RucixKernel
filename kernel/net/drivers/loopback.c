#include "net/netdevice.h"
#include "net/ethernet.h"
#include "net/skbuff.h"
#include "console.h"

 
static int loopback_xmit(struct sk_buff *skb, struct net_device *dev);

static const struct net_device_ops loopback_ops = {
    .start_xmit = loopback_xmit,
};

static void loopback_setup(struct net_device *dev) {
    dev->mtu = (16 * 1024) + 20 + 20 + 12;  
    dev->flags = IFF_LOOPBACK;
    dev->hard_header_len = ETH_HLEN;
    dev->type = ETH_P_ARP;  
    dev->dev_addr[0] = 0;
    dev->dev_addr[1] = 0;
    dev->dev_addr[2] = 0;
    dev->dev_addr[3] = 0;
    dev->dev_addr[4] = 0;
    dev->dev_addr[5] = 0;
    
    dev->netdev_ops = &loopback_ops;
}

static int loopback_xmit(struct sk_buff *skb, struct net_device *dev) {

    dev->stats.tx_packets++;
    dev->stats.tx_bytes += skb->len;

    skb->protocol = eth_type_trans(skb, dev);

    netif_rx(skb);
    
    return 0;
}

void loopback_init() {
    kprint_str("loopback_init: allocating netdev...\n");
    struct net_device *dev = alloc_netdev(0, "lo", loopback_setup);
    if (!dev) {
        kprint_str("Failed to allocate loopback device\n");
        return;
    }
    kprint_str("loopback_init: netdev allocated at ");
    kprint_hex((uint64_t)dev);
    kprint_newline();
    
    if (register_netdev(dev) < 0) {
        kprint_str("Failed to register loopback device\n");
        free_netdev(dev);
    }
}
