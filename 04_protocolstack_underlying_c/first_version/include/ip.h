#ifndef IP_H
#define IP_H

#include<stdint.h>
#define IP_P_ICMP 1
#define IP_P_TCP  6
#define IP_P_UDP  17

 struct ip_hdr{
    uint8_t  version_ihl;
    uint8_t  ds_ecn;
    uint16_t totallen;
    uint16_t numflag;
    uint16_t flag_offset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t sip;
    uint32_t dip;
 }__attribute__((packed));

#endif