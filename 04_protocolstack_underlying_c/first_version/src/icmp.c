#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>
#include"../include/icmp.h"
#include"../include/ip.h"
#include"../include/eth.h"

uint16_t checksum(void* data,size_t len){
    uint16_t *index = (uint16_t*)data;
    uint32_t sum = 0;
    uint16_t result;
    for(;len>1;len-=2){
        sum += *index++;
    }
    if(len == 1){
        sum += *(uint8_t*)index;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}
void handle_icmp(int fd,struct eth_hdr *eth,struct ip_hdr *ip,struct icmp_hdr *icmp,int len){
    if(icmp ->type != ICMP_V4_ECHO_REQUEST){
        return;
    }
    printf("          |---[收到 ICMP Ping 请求]---\n");
    uint32_t temp_ip = ip->dip;
    ip->dip = ip->sip;
    ip->sip = temp_ip;
    
    icmp ->checksum = 0;
    uint32_t icmp_len = ntohs(ip ->totallen) - (ip->version_ihl & 0x0f)*4;
    icmp->type = ICMP_V4_ECHO_REPLY;
    icmp->checksum=checksum(icmp,icmp_len);
    

    ip->checksum = 0;
    ip->checksum = checksum(ip,(ip->version_ihl & 0x0f)*4);
    uint8_t temp_mac[6];
    memcpy(temp_mac,eth->dmac,6);
    memcpy(eth->dmac,eth->smac,6);
    memcpy(eth->smac,temp_mac,6);
    if((write(fd,eth,sizeof(struct eth_hdr)+ntohs(ip->totallen)))<0){
        perror("fail to send icmp responde");
        return;
    }
    printf("          >> 已发送 ICMP Echo Reply\n");
}