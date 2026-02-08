#ifndef _NET_SOCKET_H
#define _NET_SOCKET_H

#include <stdint.h>
#include "list.h"
#include "vfs.h"

 
#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_INET   2
#define AF_INET6  10

 
#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3

 
enum {
    SS_FREE = 0,
    SS_UNCONNECTED,
    SS_CONNECTING,
    SS_CONNECTED,
    SS_DISCONNECTING
};

struct socket;
struct sockaddr;
struct msghdr;

struct proto_ops {
    int (*bind)(struct socket *sock, const struct sockaddr *uaddr, int addr_len);
    int (*connect)(struct socket *sock, const struct sockaddr *uaddr, int addr_len, int flags);
    int (*accept)(struct socket *sock, struct socket *newsock, int flags);
    int (*listen)(struct socket *sock, int len);
    int (*sendmsg)(struct socket *sock, struct msghdr *m, int total_len);
    int (*recvmsg)(struct socket *sock, struct msghdr *m, int total_len, int flags);
    int (*release)(struct socket *sock);
};

struct socket {
    short state;
    short type;
    unsigned long flags;
    struct file *file;
    struct sock *sk;
    const struct proto_ops *ops;
    struct list_head list;
};

struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};

struct sockaddr_in {
    unsigned short sin_family;
    uint16_t sin_port;
    struct in_addr {
        uint32_t s_addr;
    } sin_addr;
    char sin_zero[8];
};

struct msghdr {
    void *msg_name;
    int msg_namelen;
    struct iovec *msg_iov;
    int msg_iovlen;
    void *msg_control;
    int msg_controllen;
    int msg_flags;
};

 
int sys_socket(int domain, int type, int protocol);
int sys_bind(int fd, const struct sockaddr *addr, int addrlen);
int sys_connect(int fd, const struct sockaddr *addr, int addrlen);
int sys_listen(int fd, int backlog);
int sys_accept(int fd, struct sockaddr *addr, int *addrlen);
int sys_sendto(int fd, const void *buf, int len, int flags, const struct sockaddr *dest_addr, int addrlen);
int sys_recvfrom(int fd, void *buf, int len, int flags, struct sockaddr *src_addr, int *addrlen);

struct socket *sock_get(int fd);

void sock_init(void);

#endif  
