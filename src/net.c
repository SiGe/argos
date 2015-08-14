#define _BSD_SOURCE

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#include <linux/tcp.h>
#include <linux/sock_diag.h>
#include <linux/inet_diag.h>
#include <arpa/inet.h>
#include <pwd.h>

#include "time.h"
#include "transform.h"

#include "net.h"

//Kernel TCP states. /include/net/tcp_states.h
enum{
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_CLOSE,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_LISTEN,
    TCP_CLOSING 
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

//Copied from libmnl source
#define SOCKET_BUFFER_SIZE (getpagesize() < 8192L ? getpagesize() : 8192L)

//Example of diag_filtering, checks if destination port is <= 1000
//
//The easies way to understand filters, is to look at how the kernel
//processes them. This is done in the function inet_diag_bc_run() in
//inet_diag.c. The yes/no contains offsets to the next condition or aborts
//the loop by making the variable len in inet_diag_bc_run() negative. There
//are some limitations to the yes/no values, see inet_diag_bc_audit();
/*
unsigned char create_filter(void **filter_mem){
    struct inet_diag_bc_op *bc_op = NULL;
    unsigned char filter_len = sizeof(struct inet_diag_bc_op)*2;
    if((*filter_mem = calloc(filter_len, 1)) == NULL)
        return 0;

    bc_op = (struct inet_diag_bc_op*) *filter_mem; 
    bc_op->code = INET_DIAG_BC_D_LE;
    bc_op->yes = sizeof(struct inet_diag_bc_op)*2;
    //Only way to stop loop is to make len negative
    bc_op->no = 12;

    //For a port check, the port to check for is stored in the no field of a
    //follow-up bc_op-struct.
    bc_op = bc_op+1;
    bc_op->no = 1000;

    return filter_len;
}*/

int send_diag_msg(int sockfd){
    struct msghdr msg;
    struct nlmsghdr nlh;
    //To request information about unix sockets, this would be replaced with
    //unix_diag_req, packet-sockets packet_diag_req.
    struct inet_diag_req_v2 conn_req;
    struct sockaddr_nl sa;
    struct iovec iov[4];
    int retval = 0;

    //For the filter
    void *filter_mem = NULL;

    memset(&msg, 0, sizeof(msg));
    memset(&sa, 0, sizeof(sa));
    memset(&nlh, 0, sizeof(nlh));
    memset(&conn_req, 0, sizeof(conn_req));

    //No need to specify groups or pid. This message only has one receiver and
    //pid 0 is kernel
    sa.nl_family = AF_NETLINK;

    //Address family and protocol we are interested in. sock_diag can also be 
    //used with UDP sockets, DCCP sockets and Unix sockets, to mention a few.
    //This example requests information about TCP sockets bound to IPv4
    //addresses.
    conn_req.sdiag_family = AF_INET;
    conn_req.sdiag_protocol = IPPROTO_TCP;

    //Filter out some states, to show how it is done
    conn_req.idiag_states = TCPF_ALL & 
        ~((1<<TCP_SYN_RECV) | (1<<TCP_TIME_WAIT) | (1<<TCP_CLOSE));

    //Request extended TCP information (it is the tcp_info struct)
    //ext is a bitmask containing the extensions I want to acquire. The values
    //are defined in inet_diag.h (the INET_DIAG_*-constants).
    conn_req.idiag_ext |= (1 << (INET_DIAG_INFO - 1));
    
    nlh.nlmsg_len = NLMSG_LENGTH(sizeof(conn_req));
    //In order to request a socket bound to a specific IP/port, remove
    //NLM_F_DUMP and specify the required information in conn_req.id
    nlh.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;

    //Example of how to only match some sockets
    //In order to match a single socket, I have to provide all fields
    //sport/dport, saddr/daddr (look at dump_on_icsk)
    //conn_req.id.idiag_dport=htons(443);

    //Avoid using compat by specifying family + protocol in header
    nlh.nlmsg_type = SOCK_DIAG_BY_FAMILY;
    iov[0].iov_base = (void*) &nlh;
    iov[0].iov_len = sizeof(nlh);
    iov[1].iov_base = (void*) &conn_req;
    iov[1].iov_len = sizeof(conn_req);

    //Remove the if 0 to test the filter
#if 0
    if((filter_len = create_filter(&filter_mem)) > 0){
        memset(&rta, 0, sizeof(rta));
        rta.rta_type = INET_DIAG_REQ_BYTECODE;
        rta.rta_len = RTA_LENGTH(filter_len);
        iov[2] = (struct iovec){&rta, sizeof(rta)};
        iov[3] = (struct iovec){filter_mem, filter_len};
        nlh.nlmsg_len += rta.rta_len;
    }
#endif

    //Set essage correctly
    msg.msg_name = (void*) &sa;
    msg.msg_namelen = sizeof(sa);
    msg.msg_iov = iov;
    if(filter_mem == NULL)
        msg.msg_iovlen = 2;
    else
        msg.msg_iovlen = 4;
   
    retval = sendmsg(sockfd, &msg, 0);

    if(filter_mem != NULL)
        free(filter_mem);

    return retval;
}

static uint64_t
link_tag(struct inet_diag_msg *msg) {
    uint32_t src = *((uint32_t*)msg->id.idiag_src);
    uint32_t dst = *((uint32_t*)msg->id.idiag_dst);

    return (((uint64_t)src) << 32) | dst;
}

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

        link->tag = link_tag(diag_msg);
        link->send_acc = 0;
        link->recv_acc = 0;
        link->next = 0;

        char buf[256];

        buf[0] = 0;
        strcat(buf, "proc/net/");
        strcat(buf, link->local_addr_buf);
        strcat(buf, "/");
        strcat(buf, link->remote_addr_buf);
        strcat(buf, "/sbytes");
        history_create(&link->send, buf);
        link->send->transform = transform_identity;

        buf[0] = 0;
        strcat(buf, "proc/net/");
        strcat(buf, link->local_addr_buf);
        strcat(buf, "/");
        strcat(buf, link->remote_addr_buf);
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

    uint64_t tag = link_tag(diag_msg);

    while (cur) {
        if (cur->tag == tag)
            return cur;
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
    socket_stat *sock = (socket_stat *)malloc(sizeof(socket_stat));

    sock->bytes_received = 0;
    sock->bytes_acked = 0;
    sock->inode = socket_get_inode(diag_msg);

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

    while (sock) {
        if (sock->inode == inode) {
            return sock;
        }
        sock = sock->next;
    }

    return socket_create(socks, diag_msg);
}

static void
parse_diag_msg(struct inet_diag_msg *diag_msg, int rtalen, links_snapshot *links){
    struct rtattr *attr;
    struct tcp_info *tcpi;

    link_snapshot *link = link_find_or_create(&links->links, diag_msg);
    socket_stat *sock = socket_find_or_create(&links->sockets, diag_msg);

    //Parse the attributes of the netlink message in search of the
    //INET_DIAG_INFO-attribute
    if(rtalen > 0){
        attr = (struct rtattr*) (diag_msg+1);

        while(RTA_OK(attr, rtalen)){
            if(attr->rta_type == INET_DIAG_INFO){
                //The payload of this attribute is a tcp_info-struct, so it is
                //ok to cast
                tcpi = (struct tcp_info*) RTA_DATA(attr);

                if (sock->is_new) {
                    sock->is_new = false;
                }

                if (links->sync) {
                    sock->bytes_received = tcpi->tcpi_bytes_received;
                    sock->bytes_acked = tcpi->tcpi_bytes_acked;
                }

                link->recv_acc += (tcpi->tcpi_bytes_received - sock->bytes_received);
                link->send_acc += (tcpi->tcpi_bytes_acked - sock->bytes_acked);
                link->time = links->time;

                sock->bytes_received = tcpi->tcpi_bytes_received;
                sock->bytes_acked = tcpi->tcpi_bytes_acked;

                //Output some sample data
                /*
                fprintf(stdout, "\tState: %s RTT: %gms (var. %gms) "
                        "Recv. RTT: %gms Snd_cwnd: %u/%u\n",
                        tcp_states_map[tcpi->tcpi_state],
                        (double) tcpi->tcpi_rtt/1000, 
                        (double) tcpi->tcpi_rttvar/1000,
                        (double) tcpi->tcpi_rcv_rtt/1000, 
                        tcpi->tcpi_unacked,
                        tcpi->tcpi_snd_cwnd);
                */
            }
            attr = RTA_NEXT(attr, rtalen); 
        }
    }
}

static void
links_stat (links_snapshot* links) {
    int nl_sock = 0, numbytes = 0, rtalen = 0;
    struct nlmsghdr *nlh;
    uint8_t recv_buf[SOCKET_BUFFER_SIZE];
    struct inet_diag_msg *diag_msg;

    //Create the monitoring socket
    if((nl_sock = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_INET_DIAG)) == -1){
        perror("socket: ");
        return;
    }

    //Send the request for the sockets we are interested in
    if(send_diag_msg(nl_sock) < 0){
        perror("sendmsg: ");
        close(nl_sock);
        return;
    }

    //The requests can (will in most cases) come as multiple netlink messages. I
    //need to receive all of them. Assumes no packet loss, so if the last packet
    //(the packet with NLMSG_DONE) is lost, the application will hang.
    while(1){
        numbytes = recv(nl_sock, recv_buf, sizeof(recv_buf), 0);
        links->time = unified_time();
        nlh = (struct nlmsghdr*) recv_buf;

        while(NLMSG_OK(nlh, numbytes)){
            if(nlh->nlmsg_type == NLMSG_DONE) {
                close(nl_sock);
                return;
            }

            if(nlh->nlmsg_type == NLMSG_ERROR){
                fprintf(stderr, "Error in netlink message\n");
                close(nl_sock);
                return;
            }

            diag_msg = (struct inet_diag_msg*) NLMSG_DATA(nlh);
            rtalen = nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*diag_msg));
            parse_diag_msg(diag_msg, rtalen, links);

            nlh = NLMSG_NEXT(nlh, numbytes); 
        }
    }

    close(nl_sock);
    return;
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
}

void links_persist(links_snapshot *links) {
    link_snapshot *link = links->links;

    while (link) {
        history_append(link->recv, link->time, link->recv_acc);
        history_append(link->send, link->time, link->send_acc);
        link = link->next;
    }
}

void links_snapshot_tick(links_snapshot *links) {
    links_reset(links);
    links_stat(links);
    links_persist(links);

    if (links->sync)
        links->sync = false;
}
