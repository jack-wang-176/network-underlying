#ifndef TCP_H
#define TCP_H


#include<stdint.h>
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20

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

#endif 