#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/ioctl.h>
#include<sys/types.h>
#include<netinet/ether.h>
#include<net/if.h>
#include<linux/if_ether.h>
#include<linux/if_packet.h>

int main(int argc,char *argv[]){
    int raw_socket;
    if((raw_socket =socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL)))<0){
        perror("fail to create socket");
        exit(1);
    }
    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,"eth0",IFNAMSIZ-1);
    if((ioctl(raw_socket,SIOCGIFINDEX,&ifr))<0){
        perror("fail to get eth0 index");
        exit(1);
    }
    struct sockaddr_ll sll;
    memset(&sll,0,sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = ifr.ifr_ifindex;
    if((bind(raw_socket,(struct sockaddr *)&sll,sizeof(sll)))<0){
        perror("fail to bind");
        exit(1);
    }

    if((ioctl(raw_socket,SIOCGIFFLAGS,&ifr))<0){
        perror("ioctl");
        exit(1);
    }
    ifr.ifr_flags |= IFF_PROMISC;
    if((ioctl(raw_socket,SIOCSIFFLAGS,&ifr))<0){
        perror("ioctl");
        exit(1);
    }
    unsigned char buffer[65536];
    while(1){
        int data_size = recvfrom(raw_socket,buffer,sizeof(buffer),0,NULL,NULL);
        if(data_size<0){
            perror("fail to recvfrom");
            return 1;
        }
        struct ethhdr *eth = (struct ethhdr*)buffer;
        printf("\n=== Captured Packet (Total Size: %d bytes) ===\n", data_size);
        printf("Dest Mac : %02x %02x %02x %02x %02x %02x\n",eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
        printf("Source MAC : %02x:%02x:%02x:%02x:%02x:%02x\n",eth->h_source[0], eth->h_source[1], eth->h_source[2],eth->h_source[3], eth->h_source[4], eth->h_source[5]);
        unsigned short protocol_type =ntohs(eth->h_proto);
        switch (protocol_type){
            case ETH_P_IP:
                printf("IPV4 Protocol\n");
                break;
            case ETH_P_ARP:
                printf("Arp Protocol\n");
                break;
            case ETH_P_IPV6:
                printf("ipv6 protocol\n");
                break;
            default:
                printf("Other Protocol\n");
                break;
        }
       
    }
    close(raw_socket);
    return 0;
} 