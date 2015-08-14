#ifndef _NET_H_
#define _NET_H_

#include <stdbool.h>
#include <arpa/inet.h>

#include "history.h"

typedef struct
socket_stat {
    uint64_t bytes_received;
    uint64_t bytes_acked;

    uint64_t cookie;
    uint32_t inode;
    bool is_new;
    bool used;

    struct socket_stat *next;
} socket_stat;

typedef struct
link_snapshot {
    uint64_t tag;

    history *recv;
    history *send;

    uint64_t send_acc;
    uint64_t recv_acc;
    uint64_t time;

    char local_addr_buf[INET6_ADDRSTRLEN];
    char remote_addr_buf[INET6_ADDRSTRLEN];

    struct link_snapshot *next;
} link_snapshot;

typedef struct
links_snapshot {
    link_snapshot *links;
    socket_stat *sockets;

    /* For the first time, we just take snapshots of number of bytes
     * transfered/received so far */
    bool sync;
    uint64_t time;
} links_snapshot;

void links_snapshot_create(links_snapshot **);
void links_snapshot_delete(links_snapshot *);
void links_snapshot_tick(links_snapshot *);

#endif /* _NET_H_ */
