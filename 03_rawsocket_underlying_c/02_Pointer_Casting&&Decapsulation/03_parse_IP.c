#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/ioctl.h>
#include<net/if.h>
#include<netinet/ether.h>
#include<linux/if_packet.h>
#include<linux/if_ether.h>
#include<linux/ip.h>


int main(int argc,char *argv[]){
    int raw_socket;
    if((raw_socket =socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL)))<0){
        perror("fail to build socket");
        exit(1);
    }
    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,"eth0",IF_NAMESIZE-1);
    if((ioctl(raw_socket,SIOCGIFINDEX,&ifr))<0){
        perror("fail to get index");
        exit(1);
    }
    struct sockaddr_ll sll;
    memset(&sll,0,sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);
    if((bind(raw_socket,(struct sockaddr*)&sll,sizeof(sll)))<0){
        perror("fail to bind");
        exit(1);
    }
    if((ioctl(raw_socket,SIOCGIFFLAGS,&ifr))<0){
        perror("fail to get flag");
        exit(1);
    }
    ifr.ifr_flags |= IFF_PROMISC;
    if((ioctl(raw_socket,SIOCSIFFLAGS,&ifr))<0){
        perror("fail to set flag");
        exit(1);
    }
    unsigned char buf[65535];
    while(1){
        int data_size = recvfrom(raw_socket,&buf,sizeof(buf),0,NULL,NULL);
        if(data_size<0){
            perror("fail to recvfrom");
            break;
        }
        struct ethhdr *eth = (struct ethhdr*)&buf;
        printf("\n=== Captured Packet (Total Size: %d bytes) ===\n", data_size);
        printf("Dest Mac : %02x %02x %02x %02x %02x %02x\n",eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
        printf("Source MAC : %02x:%02x:%02x:%02x:%02x:%02x\n",eth->h_source[0], eth->h_source[1], eth->h_source[2],eth->h_source[3], eth->h_source[4], eth->h_source[5]);

        unsigned short protocol_type = htons(eth->h_proto);
        switch(protocol_type){
            case ETH_P_ARP:
              printf("ARP Protocol");
              continue;
            case ETH_P_IP:
              printf("IP Protocol");
              break;
            case ETH_P_IPV6:
              printf("IPV6 Protocol");
              continue;
        }
        struct iphdr *iph  = (struct iphdr*)(buf + sizeof(struct ethhdr));
        int ip_len = iph->ihl *4;
        printf("IP Header Length :%d bytes\n",ip_len);
        printf("TTL : %d\n",iph->ttl);
        printf("Transport Protocol:");
        switch(iph->protocol){
            case 6:
             printf("TCP\n");
             break;
            case 1:
             printf("ICMP\n");
             break;
            case 17:
             printf("UDP\n");
             break;
            default: 
             printf("Unknown (%d)\n", iph->protocol);
        }
        struct sockaddr_in source,dest;
        source.sin_addr.s_addr = iph->saddr;
        dest.sin_addr.s_addr = iph->daddr;
        printf("source IP:%s\n",inet_ntoa(source.sin_addr));
        printf("dest IP:%s\n",inet_ntoa(dest.sin_addr));
    }
}