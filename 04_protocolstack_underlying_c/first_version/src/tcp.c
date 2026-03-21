#include<stdint.h>
#include<stdio.h>
#include<arpa/inet.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include"../include/ip.h"
#include"../include/tcp.h"
#include"../include/icmp.h"
#include"../include/eth.h"

void handle_tcp_data(int fd, struct eth_hdr *eth, struct ip_hdr *ip, struct tcp_hdr *tcp);

uint16_t checksum(void* data,size_t len);
uint16_t tcp_checksum(struct ip_hdr *ip,struct tcp_hdr *tcp,int tcp_len);
void handle_tcp(int fd,struct eth_hdr *eth,struct ip_hdr *ip,struct tcp_hdr *tcp){
    uint16_t src_port = ntohs(tcp ->sport);
    uint16_t des_port = ntohs(tcp ->dport);
    uint32_t seq = ntohl(tcp->seq);
    uint32_t ack = ntohl(tcp->ack);
    printf("  |---[TCP 报文解析]---\n");
    printf("      |-端口映射: %u -> %u\n", src_port, des_port);
    printf("      |-序列号(Seq): %u\n", seq);
    printf("      |-确认号(Ack): %u\n", ack);

    
    printf("      |-标志位: ");
    if (tcp->flag & TCP_SYN) printf("SYN ");
    if (tcp->flag & TCP_ACK) printf("ACK ");
    if (tcp->flag & TCP_FIN) printf("FIN ");
    if (tcp->flag & TCP_RST) printf("RST ");
    if (tcp->flag & TCP_PSH) printf("PSH ");
    printf("\n");
    fflush(stdout);

    if ((tcp->flag & TCP_SYN) && !(tcp->flag & TCP_ACK)) {
        printf("      >> 第一次握手：收到 SYN 客户端希望与我们建立连接\n");
        printf("      >> 第二次握手: 开始构建 SYN + ACK 回复\n");

        tcp -> sport = tcp -> dport;
        tcp -> dport =  htons(src_port);
        uint32_t second_ack = seq + 1;
        tcp->ack = htonl(second_ack);
        tcp->flag = TCP_ACK | TCP_SYN;
        tcp->offset_res = (5 << 4);

        uint32_t temp_ip = ip->dip;
        ip->dip = ip->sip;
        ip->sip = temp_ip;
        ip->totallen = htons(sizeof(struct ip_hdr)+sizeof(struct tcp_hdr));

        ip->checksum = 0;
        ip->checksum = checksum(ip,sizeof(struct ip_hdr));

        tcp->checksum = 0;
        tcp->checksum = tcp_checksum(ip,tcp,sizeof(struct tcp_hdr));
        if((write(fd,eth,sizeof(struct eth_hdr) + sizeof(struct ip_hdr) + sizeof(struct tcp_hdr)))<0){
            perror("fail to send SYN ACK handshake package");
        }
        printf("      >> 已发送 SYN+ACK, 期待对方的 ACK...\n");
    }
    if(tcp->flag&TCP_ACK){
        handle_tcp_data(fd, eth, ip, tcp);
    }
}

uint16_t tcp_checksum(struct ip_hdr *ip,struct tcp_hdr *tcp,int tcp_len){
    struct pre_header pre;
    pre.dip = ip->dip;
    pre.sip = ip->sip;
    pre.protocol = ip->protocol;
    pre.zero = 0;
    pre.length = htons(tcp_len);
    int total_len = tcp_len + sizeof(struct pre_header);
    uint8_t *temp = malloc(total_len);
    memset(temp,0,total_len);
    memcpy(temp,&pre,sizeof(struct pre_header));
    memcpy(temp+sizeof(struct pre_header),tcp,tcp_len);

    uint16_t sum = checksum(temp,total_len);
    free(temp);
    return sum;
}
uint8_t* get_tcp_payloadlen(struct tcp_hdr *tcp, int total_ip_len, int ip_hdr_len, int *payload_len){
    int tcp_hdr_len = (tcp->offset_res>>4)*4;
    *payload_len = total_ip_len - ip_hdr_len - tcp_hdr_len;
    if(*payload_len <=0)return NULL;
    return (uint8_t*)tcp+tcp_hdr_len;
}


void handle_tcp_data(int fd,struct eth_hdr *eth, struct ip_hdr *ip,struct tcp_hdr *tcp){
    int ip_hdr_len = ((ip->version_ihl&0x0f) *4);
    int payload_len = 0;
    uint8_t *payload = get_tcp_payloadlen(tcp,ntohs(ip->totallen),ip_hdr_len,&payload_len);
    
    if(payload_len >0){
        printf("      >> 收到数据 (%d bytes): %.*s\n", payload_len, payload_len, payload);
        uint8_t temp_mac[6];
        memcpy(temp_mac,eth->dmac,6);
        memcpy(eth->dmac,eth->smac,6);
        memcpy(eth->smac,temp_mac,6);

        uint32_t temp_ip;
        temp_ip = ip->dip;
        ip->dip = ip->sip;
        ip->sip = temp_ip;
        ip->checksum = 0;
        ip->checksum = checksum(ip,ip_hdr_len);

        uint16_t temp_port;
        temp_port = tcp->dport;
        tcp->dport = tcp->sport;
        tcp->sport = temp_port;
        uint32_t old_ack = ntohl(tcp->ack);
        uint32_t old_seq = ntohl(tcp->seq);
        tcp->ack = htonl(old_seq + payload_len);
        tcp->seq = htonl(old_ack);
        tcp->flag = TCP_PSH |TCP_ACK;
        int tcp_total_len = ntohs(ip->totallen) - ip_hdr_len;
        tcp->checksum = 0;
        tcp->checksum = tcp_checksum(ip,tcp,tcp_total_len);

        if((write(fd,eth,sizeof(struct tcp_hdr)+ntohs(ip->totallen)))<0){
            perror("fail to respond tcp data package");
        }
        printf("      >> 已将数据回显\n");
    }
}