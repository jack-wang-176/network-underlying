#ifndef TCP_H
#define TCP_H


#include<stdint.h>
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20

typedef enum{
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_RCVD,
    TCP_ESTABLISHD,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSED_WAIT,
}tcp_state_t;

struct tcb{
    uint32_t saddr;
    uint32_t daddr;
    uint16_t sport;
    uint16_t dport;
    tcp_state_t state;
    uint32_t send_next;
    uint32_t recv_next;
};

struct tcp_hdr {
    uint16_t sport;
    uint16_t dport;
    uint32_t seq;
    uint32_t ack;
    uint8_t  offset_res;
    uint8_t  flag;
    uint16_t win;
    uint16_t checksum;
    uint16_t uindex;
}__attribute__((packed));

struct pre_header{
    uint32_t sip;
    uint32_t dip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t length;
}__attribute__((packed));

#endif 