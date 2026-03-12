#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include"../include/arp.h"
#include"../include/eth.h"

void handle_arp(int fd,struct eth_hdr *ethd,struct arp_hdr *arpd,unsigned char* mac){
    if (ntohs(arpd->opcode) != ARP_REQUEST) {
        return;
    }
    printf("  |---[构造 ARP Reply]---\n");
    unsigned char reply[42];
    struct eth_hdr *res_eth = (struct eth_hdr*)reply;
    struct arp_hdr *res_arp = (struct arp_hdr*)(reply + sizeof(struct eth_hdr));
    memcpy(res_eth->dmac,ethd->smac,6);
    memcpy(res_eth->smac,mac,6);
    res_eth -> ethtype = htons(ETH_P_ARP);
     
    res_arp -> hw_type = htons(ARP_HW_ETH);
    res_arp -> protocol_type = htons(ETH_P_IP);
    res_arp -> hw_len = 6;
    res_arp -> protocol_len = 4;
    res_arp -> opcode = htons(ARP_REPLY);
    memcpy(res_arp -> smac,mac,6);
    res_arp -> sip = arpd -> dip;
    memcpy(res_arp -> dmac,arpd -> smac,6);
    res_arp -> dip = arpd ->sip;
    if((write(fd,reply,sizeof(reply)))<0){
        perror("arp reply write error");
    }else{
        printf("      >> 已发送 ARP Reply\n");
    }
}