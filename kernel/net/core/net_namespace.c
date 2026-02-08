#include "net/net_namespace.h"
#include "console.h"

struct net init_net;

struct net *get_net(struct net *net) {
     
    return net;
}

void put_net(struct net *net) {
     
    (void)net;
}

void net_ns_init(void) {
    kprint_str("NetNS: Initialized\n");
}
