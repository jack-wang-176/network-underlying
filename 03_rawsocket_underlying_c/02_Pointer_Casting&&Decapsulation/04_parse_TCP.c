#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<netinet/ether.h>
#include<net/if.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/ioctl.h>
#include <ctype.h>
#include<linux/if_packet.h>
#include<linux/if_ether.h>
#include<linux/ip.h>
#include<linux/tcp.h>

int main(int argc, char*argv[]){
    int raw_socket;
    if((raw_socket = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL)))<0){
        perror("fail to build socket");
        exit(1);
    }
    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,"eth0",IF_NAMESIZE-1);
    if((ioctl(raw_socket,SIOGIFINDEX,&ifr))<0){
        perror("fail to get interface index");
        exit(1);
    }
    struct sockaddr_ll sll;
    memset(&sll,0,sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = ifr.ifr_ifindex;
    if((bind(raw_socket,(struct sockaddr*)&sll,sizeof(sll)))<0){
        perror("fail to bind");
        exit(1);
    }
    if((ioctl(raw_socket,SIOCGIFFLAGS,&ifr))<0){
        perror("fail to get flags");
        exit(1);
    }
    ifr.ifr_flags |= IFF_PROMISC;
    if((ioctl(raw_socket,SIOCSIFFLAGS,&ifr))<0){
        perror("fail to set flags");
        exit(1);
    }
    unsigned char buf[65535];
    while(1){
        int data_size = recvfrom(raw_socket,buf,sizeof(buf),0,NULL,NULL);
        if(data_size <0){
            perror("fail to recvfrom");
            continue;
        }
        struct ethhdr *eth = (struct ethhdr*)buf;
        printf("\n=== Captured Packet (Total Size: %d bytes) ===\n", data_size);
        printf("Dest Mac : %02x %02x %02x %02x %02x %02x\n",eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
        printf("Source MAC : %02x:%02x:%02x:%02x:%02x:%02x\n",eth->h_source[0], eth->h_source[1], eth->h_source[2],eth->h_source[3], eth->h_source[4], eth->h_source[5]);
        switch(ntohs(eth->h_proto)){
            case ETH_P_IP:
              printf("IP Protocol\n");
              break;
            case ETH_P_IPV6:
              printf("IPV6 Protocol\n");
              continue;
            case ETH_P_ARP:
              printf("ARP Protocl\n");
              continue;
            default:
              printf("Other Protocl\n");
              continue;    
        }
        struct iphdr *iph = (struct iphdr*)(buf +  sizeof(struct ethhdr));
        int ip_len = iph->ihl*4;
        printf("IP Header Length :%d bytes\n",ip_len);
        printf("TTL : %d\n",iph->ttl);
        printf("Transport Protocol:");
        switch(iph->protocol){
            case IPPROTO_TCP: // 
             printf("TCP\n");
             break;
            case IPPROTO_ICMP: // 1
             printf("ICMP\n");
             continue;
            case IPPROTO_UDP: // 17
             printf("UDP\n");
             continue;
            default: 
             printf("Unknown (%d)\n", iph->protocol);
             continue;
        }
        struct sockaddr_in source,dest;
        source.sin_addr.s_addr = iph->saddr;
        dest.sin_addr.s_addr = iph->daddr;
        printf("source IP:%s\n",inet_ntoa(source.sin_addr));
        printf("dest IP:%s\n",inet_ntoa(dest.sin_addr));
        struct tcphdr *tcph = (struct tcphdr*)(buf + sizeof(struct ethhdr)+ip_len);
        printf("source port :%d\n",ntohs(tcph->source));
        printf("destination port :%d\n",ntohs(tcph->dest));
        printf("Flag is");
        if (tcph->syn) printf("SYN ");
        if (tcph->ack) printf("ACK ");
        if (tcph->fin) printf("FIN ");
        if (tcph->rst) printf("RST ");
        if (tcph->psh) printf("PSH ");
        if (tcph->urg) printf("URG ");
        printf("\n");
        int tcp_header_len = tcph->doff*4;
        printf("TCP Header Length :%d bytes\n",tcp_header_len);
        int payload_offset = (sizeof(struct ethhdr)+tcp_header_len+ip_len);
        int payload_len = data_size - payload_offset;
        printf("\n[+] Entering L5 Application Layer: Payload\n");
        printf("    Payload Offset   : %d bytes from buffer start\n", payload_offset);
        
        if(payload_len == 0){
            printf("    [Payload is empty - Control Packet]\n");
        }else{
            printf("    Payload Length   : %d bytes\n", payload_len);
            printf("--------------------------------------------------\n");
            unsigned char *payload = buf+payload_offset;
            for(int i=0;i<payload_len;i++){
                if(isprint(payload[i])|| payload[i] == '\n'||payload[i] == '\r'){
                    putchar(payload[i]);
                }else{
                    putchar('.');
                }
            }
            printf("\n--------------------------------------------------\n");
        }
    }
}