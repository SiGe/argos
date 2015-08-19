#define _BSD_SOURCE
#define GROUP_TCP_ON_LINK
#define IGNORE_LOCAL_CONNECTIONS

#include <assert.h>
#include <inttypes.h>
#include <pcap/pcap.h>
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

typedef struct __attribute__((packed))
tcp_packet {
    // 14 bytes
    uint32_t eth_smac_u;
    uint16_t eth_smac_l;
    uint32_t eth_dmac_u;
    uint16_t eth_dmac_l;
    uint16_t eth_ethertype;
    
    // 5 * 32 bits
    // Remember that network byte order is the other way around
#ifdef __LITTLE_ENDIAN_BITFIELD
    unsigned int ip_ihl : 4;
    unsigned int ip_version : 4;
#endif

    // 20 bytes
    uint8_t ip_tos;
    uint16_t ip_len;
    uint16_t ip_ident;
    unsigned int ip_flags : 3;
    unsigned int ip_frag : 13;
    uint8_t ip_ttl;
    uint8_t ip_proto;
    uint16_t ip_chksum;
    uint32_t ip_src;
    uint32_t ip_dst;

    // 5 * 32 bits
    uint16_t tcp_sport;
    uint16_t tcp_dport;
    uint32_t tcp_seq;
    uint32_t tcp_ack;

    unsigned int tcp_reserve: 3;
    unsigned int tcp_ns : 1;
    unsigned int tcp_offset : 4;

    union {
        struct __attribute__((packed)) {
            uint8_t flags;
        } packed;

        struct __attribute__((packed)) {
            unsigned int tcp_cwr : 1;
            unsigned int tcp_ere : 1;
            unsigned int tcp_urg : 1;
            unsigned int tcp_ack : 1;
            unsigned int tcp_psh : 1;
            unsigned int tcp_rst : 1;
            unsigned int tcp_syn : 1;
            unsigned int tcp_fin : 1;
        } unpacked;
    } tcp_flags;

    uint16_t tcp_wndsize;
    uint16_t tcp_chksum;
    uint16_t tcp_ptr;
} tcp_packet;

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
link_create(uint32_t _src, uint32_t _dst, uint16_t _sport, uint16_t _dport) {
    link_snapshot *link = (link_snapshot *)malloc(sizeof(link_snapshot));

    memset(link->local_addr_buf, 0, sizeof(link->local_addr_buf));
    memset(link->remote_addr_buf, 0, sizeof(link->remote_addr_buf));

    inet_ntop(AF_INET, (struct in_addr*) &_src, 
        link->local_addr_buf, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, (struct in_addr*) &_dst, 
        link->remote_addr_buf, INET_ADDRSTRLEN);

    link->srcip = _src;
    link->dstip = _dst;
    link->sport = _sport;
    link->dport = _dport;

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

    //printf("Creating link -send (%p): %s\n", link, buf);

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

    //printf("Creating link -recv (%p): %s\n", link, buf);
    return link;
}

static link_snapshot*
link_find_or_create(link_snapshot **links, uint32_t src, uint32_t dst, uint32_t sport, uint32_t dport) {
    link_snapshot *cur = *links;

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

    cur = link_create(src, dst, sport, dport);
    cur->next = *links;
    *links = cur;

    return cur;
}

static void
packets_process(u_char *data, struct pcap_pkthdr* const pkthdr, u_char* const pkt_data) {
    links_snapshot *links = (links_snapshot *)data;
    if ((pkthdr->len < sizeof(tcp_packet)) || (pkthdr->caplen < sizeof(tcp_packet))) return;

    tcp_packet *pkt = (struct tcp_packet *)pkt_data;

    // Reverse byte order
    if (pkt->eth_ethertype != 0x008) return;
    if (pkt->ip_proto != 0x06) return;

    uint16_t len = ntohs(pkt->ip_len);

    if ((len - (pkt->tcp_offset * 4)) <= (sizeof(tcp_packet)-14)) return;

    link_snapshot *link = link_find_or_create(&(links->links), pkt->ip_src, pkt->ip_dst, pkt->tcp_sport, pkt->tcp_dport);
    link->send_acc += (len - 14 - 20 - 4*pkt->tcp_offset);
}

static void* 
packets_loop(void *data) {
    char errbuf[PCAP_ERRBUF_SIZE];
    char *devname = pcap_lookupdev(errbuf);

    if (!devname)
    {printf("Could not find a suitable network device to monitor.\n"); return 0;}

    links_snapshot *links = (links_snapshot *)data;
    links->dev = pcap_open_live(devname, 64, false, -1, errbuf);

    printf("Trying to monitor: %s\n", devname);
    if (!links->dev)
    {printf("Could not open the device to capture packets from: %s", devname); return 0;}

    printf("Monitoring: %s\n", devname);

    /* Loop forever while capturing packets */
    pcap_loop(links->dev, -1, packets_process, data);

    return 0;
}


void links_snapshot_create(links_snapshot **links) {
    *links = (links_snapshot*)(malloc(sizeof(links_snapshot)));
    (*links)->links = 0;
    (*links)->time = 0;
    if (pthread_create(&(*links)->thread , 0, &packets_loop, *links) < 0)
        printf("Failed to create network monitoring loop.\n");
}

void links_snapshot_delete(links_snapshot *links) {
    /* Stop capturing packets */
    if (links->dev) {
        pcap_breakloop(links->dev);
    }

    pthread_join(links->thread, 0);

    link_snapshot *link = links->links;
    link_snapshot *tmp = link;

    while (link) {
        tmp = link->next;
        link_delete(link);
        link = tmp;
    }

    free(links);
}

void links_persist(links_snapshot *links) {
    link_snapshot *link = links->links;

    while (link) {
        link->time = unified_time();
        /* If link was used during the past epoch save it's statistics */
        if (link->time != 0) {
            history_append(link->recv, link->time, link->recv_acc);
            history_append(link->send, link->time, link->send_acc);
            link->time = 0;
            link->send_acc = 0;
            link->recv_acc = 0;
        }
        link = link->next;
    }
}

void links_snapshot_tick(links_snapshot *links) {
    links_persist(links);

    if (links->sync)
        links->sync = false;
}
