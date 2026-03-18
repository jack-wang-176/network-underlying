#ifndef ETH_H
#define ETH_H


#include<stdint.h>

#define ETH_P_IP 0x0800
#define ETH_P_ARP 0x0806

struct eth_hdr {
    uint8_t  dmac[6];
    uint8_t  smac[6];
    uint16_t ethtype;
}__attribute__((packed));
#endif