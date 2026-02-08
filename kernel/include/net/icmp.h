#ifndef _NET_ICMP_H
#define _NET_ICMP_H

#include <stdint.h>
#include "net/skbuff.h"

#define ICMP_ECHOREPLY      0
#define ICMP_DEST_UNREACH   3
#define ICMP_SOURCE_QUENCH  4
#define ICMP_REDIRECT       5
#define ICMP_ECHO           8
#define ICMP_TIME_EXCEEDED  11
#define ICMP_PARAMETERPROB  12
#define ICMP_TIMESTAMP      13
#define ICMP_TIMESTAMPREPLY 14
#define ICMP_INFO_REQUEST   15
#define ICMP_INFO_REPLY     16

struct icmphdr {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    union {
        struct {
            uint16_t id;
            uint16_t sequence;
        } echo;
        uint32_t gateway;
        struct {
            uint16_t __unused;
            uint16_t mtu;
        } frag;
    } un;
} __attribute__((packed));

void icmp_init(void);
void icmp_rcv(struct sk_buff *skb);

#endif  
