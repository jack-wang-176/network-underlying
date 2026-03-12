#ifndef ARP_H
#define ARP_H


#include<stdint.h>

#define ARP_REQUEST 1
#define ARP_REPLY 2

#define ARP_HW_ETH 1

struct arp_hdr{
    uint16_t  hw_type;
    uint16_t  protocol_type;
    uint8_t   hw_len;
    uint8_t   protocol_len;
    uint16_t  opcode;
    
    uint8_t  smac[6];
    uint32_t sip;
    uint8_t  dmac[6];
    uint32_t dip;
}__attribute__((packed));
#endif