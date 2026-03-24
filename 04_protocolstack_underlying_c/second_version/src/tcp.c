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

struct tcp cur_tcb = {
    .state = TCP_LISTEN
};

void handle_tcp_data(int fd, struct eth_hdr *eth, struct ip_hdr *ip, struct tcp_hdr *tcp);
struct tcp tcp_conn[MAX_CONN];
struct tcp* find_tcp(uint32_t saddr,uint32_t daddr, uint16_t sport,uint16_t dport){
  for(int i =0;i<MAX_CONN;i++){
    if(tcp_conn[i].usued&&tcp_conn[i].daddr == daddr&&tcp_conn[i].saddr== saddr
     &&tcp_conn[i].sport == sport&&tcp_conn[i].dport == dport
    )return &tcp_conn[i];
  }
  return NULL;
}
struct tcp*alloc_tcp(uint32_t saddr,uint32_t daddr, uint16_t sport,uint16_t dport){
  for(int i=0;i<MAX_CONN;i++){
    if(!tcp_conn[i].usued){
      tcp_conn[i].usued = true;
      tcp_conn[i].daddr = daddr;
      tcp_conn[i].saddr = saddr;
      tcp_conn[i].dport = dport;
      tcp_conn[i].sport = sport;
      tcp_conn[i].state = TCP_SYN_RCVD;
      return &tcp_conn[i];
    }
    
  }
  return NULL;
}

uint16_t checksum(void* data,size_t len);
uint16_t tcp_checksum(struct ip_hdr *ip,struct tcp_hdr *tcp,int tcp_len);
uint8_t* get_tcp_payloadlen(struct tcp_hdr *tcp, int total_ip_len, int ip_hdr_len, int *payload_len);
void reply_tcp_packet(int fd,struct eth_hdr *eth,struct ip_hdr *ip,struct tcp_hdr *tcp,int payload_len){
    uint8_t temp_mac[6];
    memcpy(temp_mac,eth->dmac,6);
    memcpy(eth->dmac,eth->smac,6);
    memcpy(eth->smac,temp_mac,6);

    uint32_t temp_ip;
    temp_ip = ip->dip;
    ip->dip = ip->sip;
    ip->sip = temp_ip;
    ip->checksum = 0;
    ip->checksum = checksum(ip,(ip->version_ihl&0x0f)*4);
    
    uint16_t temp_port = tcp->dport;
    tcp->dport = tcp->sport;
    tcp->sport = temp_port;
    tcp->checksum = 0;
    tcp->checksum = tcp_checksum(ip,tcp,(tcp->offset_res>>4)*4+payload_len);

    if(write(fd,eth,sizeof(struct eth_hdr) + ntohs(ip->totallen))<0){
        perror("fail to react fin");
    }
}
void handle_tcp(int fd,struct eth_hdr *eth,struct ip_hdr *ip,struct tcp_hdr *tcp){
  
    uint32_t incoming_seq = ntohl(tcp->seq);
    uint32_t incoming_ack = ntohl(tcp->ack);
    int ip_hdr_len =(ip->version_ihl & 0x0f)*4;
    int payload_len;
    get_tcp_payloadlen(tcp,ntohs(ip->totallen),ip_hdr_len,&payload_len);

    uint32_t saddr = ip->sip;
    uint32_t daddr = ip->dip;
    uint16_t sport = ntohs(tcp->sport);
    uint16_t dport = ntohs(tcp->dport);

    struct tcp *cur_tcb = find_tcp(saddr,daddr,sport,dport);
    if(cur_tcb ==NULL&&tcp->flag &TCP_SYN){
      printf("      >>收到第一次握手数据包,开始初始化pcb并开始构建第二次握手数据包\n");
      cur_tcb = alloc_tcp(saddr,daddr,sport,dport);
      cur_tcb->recv_next = incoming_seq+1;
      cur_tcb->send_next = 5658;
      
      ip->totallen = htons(sizeof(struct tcp_hdr)+sizeof(struct ip_hdr));
      tcp->flag = TCP_ACK |TCP_SYN;
      tcp->seq = htonl(cur_tcb->send_next);
      tcp->ack = htonl(cur_tcb->recv_next);
      tcp->offset_res = (5<<4);
      reply_tcp_packet(fd,eth,ip,tcp,0);
      cur_tcb->send_next++;
      cur_tcb->state = TCP_SYN_RCVD;
      printf("      >> 状态切换 -> TCP_SYN_RCVD\n");
      return;
    }
    if (cur_tcb == NULL) {
        printf("  |- 收到未知的 TCP 包 (非 SYN 且无记录)，静默丢弃。\n");
        return; // 直接丢弃，绝对不能往下走到 switch！
    }

    printf("  |---[TCP 状态机] 当前状态: %d ---\n", cur_tcb->state);
    switch(cur_tcb->state){
        case TCP_SYN_RCVD:
          if(tcp->flag&TCP_ACK){
            printf("      >>收到第三次次握手数据包,检验数据包并切换状态SYN_SENT\n");
            if(incoming_ack == cur_tcb->send_next){
                printf("      >> [SYN_RCVD] 收到握手 ACK，连接建立！\n");
                cur_tcb->state = TCP_ESTABLISHED;
            }
          }
          break;
        case TCP_ESTABLISHED:
          if(tcp->flag&(TCP_ACK)&&payload_len >0){
            printf("      >> [ESTABLISHED] 收到数据 (%d bytes)\n", payload_len);
            cur_tcb->recv_next = incoming_seq + payload_len;

            tcp->flag = TCP_ACK | TCP_PSH;
            tcp->seq = htonl(cur_tcb->send_next);
            tcp->ack = htonl(cur_tcb->recv_next);
            reply_tcp_packet(fd,eth,ip,tcp,payload_len);
            cur_tcb->send_next += payload_len;
            printf("      >> 数据已回显，更新 TCB 序列号\n");
          }
          if(tcp->flag&TCP_FIN){
            printf("      >> [ESTABLISHED] 收到 FIN，开始挥手\n");
            cur_tcb->recv_next = incoming_seq +1;
            tcp->seq = htonl(cur_tcb->send_next);
            tcp->ack = htonl(cur_tcb->recv_next);
            tcp->flag = TCP_ACK |TCP_FIN;
            ip->totallen =htons(sizeof(struct ip_hdr)+sizeof(struct tcp_hdr));
            tcp->offset_res = (5<<4);
            reply_tcp_packet(fd,eth,ip,tcp,0);
            cur_tcb->send_next++;
            cur_tcb->state = TCP_LAST_ACK;      // 状态转移
            printf("      >> 已连发 ACK 和 FIN+ACK，状态切换 -> TCP_LAST_ACK\n");
          }
          break;
        case TCP_LAST_ACK:
          if(tcp->flag & TCP_ACK && incoming_ack == cur_tcb->send_next){
            printf("      >> [LAST_ACK] 收到客户端的最终 ACK，挥手彻底完成，连接关闭！\n");
            cur_tcb->state = TCP_LISTEN; 
            printf("      >> 状态切换 -> TCP_LISTEN (等待新连接)\n");
          }
          cur_tcb->usued = false;
          break;
        default:
          printf("      >> 未知或未处理的状态: %d\n", cur_tcb->state);
          break;

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


