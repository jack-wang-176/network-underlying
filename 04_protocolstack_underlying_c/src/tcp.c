#include<stdint.h>
#include<stdio.h>
#include<arpa/inet.h>
#include"../include/ip.h"
#include"../include/tcp.h"

void handle_tcp(struct tcp_hdr *tcp){
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

    if ((tcp->flag & TCP_SYN) && !(tcp->flag & TCP_ACK)) {
        printf("      >> 第一次握手：收到 SYN 客户端希望与我们建立连接\n");
    }
}