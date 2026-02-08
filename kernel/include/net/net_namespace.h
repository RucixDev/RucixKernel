#ifndef _NET_NAMESPACE_H
#define _NET_NAMESPACE_H

#include <stdint.h>
#include "net/netdevice.h"

struct net {
    struct net_device *dev_base;

    struct net_device *loopback_dev; 
};

 
extern struct net init_net;

struct net *get_net(struct net *net);
void put_net(struct net *net);

#endif  
