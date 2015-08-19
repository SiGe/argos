#define _BSD_SOURCE
#define GROUP_TCP_ON_LINK
#define IGNORE_LOCAL_CONNECTIONS

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <arpa/inet.h>

#include <asm/types.h>
#include <libmnl/libmnl.h>
#include <linux/inet_diag.h>
#include <linux/sock_diag.h>
#include <linux/tcp.h>


#include "time.h"
#include "transform.h"

#include "net.h"

//Kernel TCP states. /include/net/tcp_states.h
enum {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT, // 6
    TCP_CLOSE,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_LISTEN, // 10
    TCP_CLOSING,    /* Now a valid state */
    TCP_NEW_SYN_RECV,

    TCP_MAX_STATES  /* Leave at the end! */
};

/*
static const char* tcp_states_map[]={
    [TCP_ESTABLISHED] = "ESTABLISHED",
    [TCP_SYN_SENT] = "SYN-SENT",
    [TCP_SYN_RECV] = "SYN-RECV",
    [TCP_FIN_WAIT1] = "FIN-WAIT-1",
    [TCP_FIN_WAIT2] = "FIN-WAIT-2",
    [TCP_TIME_WAIT] = "TIME-WAIT",
    [TCP_CLOSE] = "CLOSE",
    [TCP_CLOSE_WAIT] = "CLOSE-WAIT",
    [TCP_LAST_ACK] = "LAST-ACK",
    [TCP_LISTEN] = "LISTEN",
    [TCP_CLOSING] = "CLOSING"
};
*/

//There are currently 11 states, but the first state is stored in pos. 1.
//Therefore, I need a 12 bit bitmask
#define TCPF_ALL 0xFFF

static void
link_delete(link_snapshot *link) {
    history_save(link->recv);
    history_save(link->send);

    history_delete(link->recv);
    history_delete(link->send);

    free(link);
}

static link_snapshot *
link_create(struct inet_diag_msg *diag_msg) {
    link_snapshot *link = (link_snapshot *)malloc(sizeof(link_snapshot));

    memset(link->local_addr_buf, 0, sizeof(link->local_addr_buf));
    memset(link->remote_addr_buf, 0, sizeof(link->remote_addr_buf));

    if(diag_msg->idiag_family == AF_INET){
        inet_ntop(AF_INET, (struct in_addr*) &(diag_msg->id.idiag_src), 
            link->local_addr_buf, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, (struct in_addr*) &(diag_msg->id.idiag_dst), 
            link->remote_addr_buf, INET_ADDRSTRLEN);

        link->srcip = diag_msg->id.idiag_src[0];
        link->dstip = diag_msg->id.idiag_dst[0];
        link->sport = diag_msg->id.idiag_sport;
        link->dport = diag_msg->id.idiag_dport;

        link->send_acc = 0;
        link->recv_acc = 0;
        link->next = 0;

        char buf[256];

        char sport[256];
        char dport[256];

        memset(sport, 0, 256);
        memset(dport, 0, 256);

        snprintf(sport, 256, "%d", ntohs(link->sport));
        snprintf(dport, 256, "%d", ntohs(link->dport));

        buf[0] = 0;
        strcat(buf, "proc/net/");
        strcat(buf, link->local_addr_buf);
        strcat(buf, "/");
        strcat(buf, link->remote_addr_buf);
#ifndef GROUP_TCP_ON_LINK
        strcat(buf, "/");
        strcat(buf, sport);
        strcat(buf, "/");
        strcat(buf, dport);
#endif
        strcat(buf, "/sbytes");
        history_create(&link->send, buf);
        link->send->transform = transform_identity;

        buf[0] = 0;
        strcat(buf, "proc/net/");
        strcat(buf, link->local_addr_buf);
        strcat(buf, "/");
        strcat(buf, link->remote_addr_buf);
#ifndef GROUP_TCP_ON_LINK
        strcat(buf, "/");
        strcat(buf, sport);
        strcat(buf, "/");
        strcat(buf, dport);
#endif
        strcat(buf, "/rbytes");
        history_create(&link->recv, buf);
        link->recv->transform = transform_identity;

    } else {
        /* We do not handle ipv6 nor other protocols */
        fprintf(stderr, "Unknown family\n");
        free(link);
        return 0;
    }
    return link;
}

static link_snapshot*
link_find_or_create(link_snapshot **links, struct inet_diag_msg *diag_msg) {
    link_snapshot *cur = *links;

    uint32_t src = diag_msg->id.idiag_src[0];
    uint32_t dst = diag_msg->id.idiag_dst[0];

#ifndef GROUP_TCP_ON_LINK
    uint32_t sport = diag_msg->id.idiag_sport;
    uint32_t dport = diag_msg->id.idiag_dport;
#endif

    while (cur) {
#ifdef GROUP_TCP_ON_LINK
        if ((cur->srcip == src) &&
                (cur->dstip == dst))
            return cur;
#else
        if ((cur->srcip == src) &&
                (cur->dstip == dst) &&
                (cur->sport == sport) &&
                (cur->dport == dport))
            return cur;
#endif
        cur = cur->next;
    }

    cur = link_create(diag_msg);
    cur->next = *links;
    *links = cur;

    return cur;
}

static uint32_t
socket_get_inode(struct inet_diag_msg *msg) {
    return msg->idiag_inode;
}

static socket_stat *
socket_create(socket_stat **socks, struct inet_diag_msg *diag_msg) {
    // If the connection is already closed, we aren't interested in
    // it.
    //
    // XXX: Truth is, we actually might be, if the life of a connection is
    // shorter than EPOCH (defined in main.c) seconds.
    if (diag_msg->idiag_state == TCP_CLOSE) {
        return 0;
    }

    socket_stat *sock = (socket_stat *)malloc(sizeof(socket_stat));

    sock->bytes_received = 0;
    sock->bytes_acked = 0;

    sock->last_sent = 0;
    sock->last_recv = 0;

    sock->inode = socket_get_inode(diag_msg);
    sock->srcip = diag_msg->id.idiag_src[0];
    sock->dstip = diag_msg->id.idiag_dst[0];
    sock->sport = diag_msg->id.idiag_sport;
    sock->dport = diag_msg->id.idiag_dport;

    sock->next = *socks;
    *socks = sock;

    return sock;
}

static void
socket_delete(socket_stat *socks) {
    free(socks);
}

static socket_stat *
socket_find_or_create(socket_stat **socks, struct inet_diag_msg *diag_msg) {
    socket_stat *sock = *socks;

    uint32_t inode = socket_get_inode(diag_msg);
    uint32_t src = diag_msg->id.idiag_src[0];
    uint32_t dst = diag_msg->id.idiag_dst[0];
    uint32_t sport = diag_msg->id.idiag_sport;
    uint32_t dport = diag_msg->id.idiag_dport;

    while (sock) {
        if ((sock->inode == inode) && 
                (sock->srcip == src) &&
                (sock->dstip == dst) &&
                (sock->sport == sport) &&
                (sock->dport == dport)) {
            return sock;
        }
        sock = sock->next;
    }

    return socket_create(socks, diag_msg);
}

static void
log_socket(struct inet_diag_msg *diag_msg, uint64_t time) {
    char buf[256] = {0};
    char src[16] = {0};
    char dst[16] = {0};

    char sport[8] = {0}; 
    char dport[8] = {0}; 

    char state[4] = {0};

    inet_ntop(AF_INET, (struct in_addr*) &(diag_msg->id.idiag_src), 
        src, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, (struct in_addr*) &(diag_msg->id.idiag_dst), 
        dst, INET_ADDRSTRLEN);

    snprintf(sport, 8, "%d", ntohs(diag_msg->id.idiag_sport));
    snprintf(dport, 8, "%d", ntohs(diag_msg->id.idiag_dport));
    snprintf(state, 4, "%d", diag_msg->idiag_state);

    strcat(buf, "%" PRIu64);
    strcat(buf, " - ");
    strcat(buf, src);
    strcat(buf, ":");
    strcat(buf, sport);
    strcat(buf, "/");
    strcat(buf, dst);
    strcat(buf, ":");
    strcat(buf, dport);
    strcat(buf, " --> ");
    strcat(buf, state);
    strcat(buf, "\n");

    printf(buf, time);
}

static int
inet_diag_msg_type(struct nlattr const *nlattr, void *data) {
    if (mnl_attr_get_type(nlattr) != INET_DIAG_INFO)
        return MNL_CB_OK;

    const struct tcp_info **tcpi = data;
    *tcpi = mnl_attr_get_payload(nlattr);
    return MNL_CB_OK;
}

static int
inet_diag_msg_history(struct tcp_info *tcpi, struct inet_diag_msg *diag_msg, links_snapshot *links) {
#ifdef IGNORE_LOCAL_CONNECTIONS
    if (diag_msg->id.idiag_src[0] == diag_msg->id.idiag_dst[0])
        return 0;
#endif

    link_snapshot *link = link_find_or_create(&links->links, diag_msg);
    socket_stat *sock = socket_find_or_create(&links->sockets, diag_msg);

    log_socket(diag_msg, links->time);
    /* If this is the first time that we are syncing the
     * link, just remember the tcpi_bytes_received and
     * tcpi_bytes_acked */
    if (links->sync) {
        sock->bytes_received = tcpi->tcpi_bytes_received;
        sock->bytes_acked = tcpi->tcpi_bytes_acked;
    }

    /* If the last data sent is older than the data that we
     * remembered update the counters */
    //if (tcpi->tcpi_last_data_sent > sock->last_sent) {
    link->send_acc += (tcpi->tcpi_bytes_acked - sock->bytes_acked);
    sock->last_sent = tcpi->tcpi_last_data_sent;
    sock->bytes_acked = tcpi->tcpi_bytes_acked;
    //}

    //if (tcpi->tcpi_last_data_recv > sock->last_recv) {
    link->recv_acc += (tcpi->tcpi_bytes_received - sock->bytes_received);
    sock->last_recv = tcpi->tcpi_last_data_recv;
    sock->bytes_received = tcpi->tcpi_bytes_received;
    //}

    link->time = links->time;
    sock->used = true;
    return 0;
}

static int
inet_diag_parse(struct nlmsghdr const *nlh, void *data){
    struct inet_diag_msg *msg = mnl_nlmsg_get_payload(nlh);
    char src[17] = {0};
    char dst[17] = {0};

    inet_ntop(AF_INET, (void const*)msg->id.idiag_src, src, 16);
    inet_ntop(AF_INET, (void const*)msg->id.idiag_dst, dst, 16);

    struct tcp_info *tcpi = 0;
    mnl_attr_parse(nlh, sizeof(struct inet_diag_msg), inet_diag_msg_type, &tcpi);

    if (!tcpi)
        return MNL_CB_OK;

    links_snapshot *links = (void *)data;
    links->time = unified_time();
    inet_diag_msg_history(tcpi, msg, links);

    return MNL_CB_OK;
}

static void
links_stat (links_snapshot* links) {
	int ret;
	unsigned int seq, portid;

    struct mnl_socket *nl;
	struct nlmsghdr *nlh;
    struct inet_diag_req_v2* conn_req;

	char buf[MNL_SOCKET_BUFFER_SIZE];

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= SOCK_DIAG_BY_FAMILY; //RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = seq = time(NULL);

    conn_req = mnl_nlmsg_put_extra_header(nlh, sizeof(struct inet_diag_req_v2));
    conn_req->idiag_ext = (1 << (INET_DIAG_INFO -1));
    conn_req->sdiag_family = AF_INET;
    conn_req->sdiag_protocol = IPPROTO_TCP;

    //Filter->out some states, to show how it is done
    conn_req->idiag_states = 0xfff;

	nl = mnl_socket_open(NETLINK_INET_DIAG);
	if (nl == NULL) {
		perror("mnl_socket_open");
		exit(EXIT_FAILURE);
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		exit(EXIT_FAILURE);
	}
	portid = mnl_socket_get_portid(nl);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_sendto");
		exit(EXIT_FAILURE);
	}

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, seq, portid, inet_diag_parse, (void *)links);
		if (ret <= MNL_CB_STOP)
			break;
		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	}

	if (ret == -1) {
		perror("error");
		exit(EXIT_FAILURE);
	}

	mnl_socket_close(nl);
}

void links_snapshot_create(links_snapshot **links) {
    *links = (links_snapshot*)(malloc(sizeof(links_snapshot)));
    (*links)->links = 0;
    (*links)->time = 0;
    (*links)->sockets = 0;
}

void links_snapshot_delete(links_snapshot *links) {
    link_snapshot *link = links->links;
    link_snapshot *tmp = link;
    socket_stat *sock = links->sockets;
    socket_stat *socktmp = links->sockets;

    while (link) {
        tmp = link->next;
        link_delete(link);
        link = tmp;
    }

    while (sock) {
        socktmp = sock->next;
        socket_delete(sock);
        sock = socktmp;
    }

    free(links);
}

void links_reset(links_snapshot *links) {
    link_snapshot *link = links->links;

    while (link) {
        link->send_acc = 0;
        link->recv_acc = 0;
        link = link->next;
    }

    socket_stat *sock = links->sockets;
    while (sock) {
        sock = sock->next;
    }
}

void links_persist(links_snapshot *links) {
    link_snapshot *link = links->links;

    while (link) {
        /* If link was used during the past epoch save it's statistics */
        if (link->time != 0) {
            history_append(link->recv, link->time, link->recv_acc);
            history_append(link->send, link->time, link->send_acc);
            link->time = 0;
        }
        link = link->next;
    }
}

void links_snapshot_tick(links_snapshot *links) {
    links_reset(links);
    links_stat(links);
    printf("----------------------------------------\n");
    links_persist(links);

    if (links->sync)
        links->sync = false;
}
