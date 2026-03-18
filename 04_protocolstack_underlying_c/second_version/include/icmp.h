#ifndef ICMP_H
#define ICMP_H

#include<stdint.h>

#define ICMP_V4_ECHO_REQUEST 8
#define ICMP_V4_ECHO_REPLY 0


struct icmp_hdr{
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t flagnum;
    uint16_t seqnum;
}__attribute__((packed));
#endif