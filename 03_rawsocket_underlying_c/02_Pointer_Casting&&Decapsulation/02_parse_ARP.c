#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/ioctl.h>
#include<arpa/inet.h>
#include<linux/if_packet.h>
#include<linux/if_ether.h>
#include<net/if.h>
#include<netinet/ether.h>

int main(int argc, char*argv[]){
    int raw_socket;
    if((raw_socket = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL)))<0){
        perror("fail to build socket");
        exit(1);
    }

    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,"eth0",IF_NAMESIZE-1);
    if((ioctl(raw_socket,SIOCGIFINDEX,&ifr))<0){
        perror("fail to get interface id");
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
        perror("fail to get flag");
        exit(1);
    }
    ifr.ifr_flags |= IFF_PROMISC;
    if((ioctl(raw_socket,SIOCSIFFLAGS,&ifr))<0){
        perror("fail to set flag");
        exit(1);
    }
    unsigned char buf[65536];
    while(1){
        int data_size = recvfrom(raw_socket,buf,sizeof(buf),0,NULL,NULL);
        if(data_size<0){
            perror("fail to recvfrom");
            return 1;
        }
        struct ethhdr *eth = (struct ethhdr*)buf;
        printf("\n=== Captured Packet (Total Size: %d bytes) ===\n", data_size);
        printf("Dest Mac : %02x %02x %02x %02x %02x %02x\n",eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
        printf("Source MAC : %02x:%02x:%02x:%02x:%02x:%02x\n",eth->h_source[0], eth->h_source[1], eth->h_source[2],eth->h_source[3], eth->h_source[4], eth->h_source[5]);
        unsigned short protocol_type = ntohs(eth->h_proto);
        switch (protocol_type){
            case ETH_P_IP: 
               printf("IPV4 Protocol\n");
               continue;
            case ETH_P_IPV6:
               printf("IPV6 Protocol\n");
               continue;
            case ETH_P_ARP:
               printf("ARP Protocol\n");
               break;
        }
        struct ether_arp *arp = (struct ether_arp*)(buf + sizeof(struct ethhdr));
        unsigned short opcode = htons(arp->ea_hdr.ar_op);
        switch(opcode){
            case ARPOP_REPLY:
              printf("arp reply");
              break;
            case ARPOP_REQUEST:
              printf("arp request");
              break;
            default:
              printf("unkowned arp op");
              break;
        }
        printf("    Sender IP  : %d.%d.%d.%d\n",
           arp->arp_spa[0], arp->arp_spa[1], arp->arp_spa[2], arp->arp_spa[3]);
           
        printf("    Sender MAC : %02x:%02x:%02x:%02x:%02x:%02x\n",
           arp->arp_sha[0], arp->arp_sha[1], arp->arp_sha[2],
           arp->arp_sha[3], arp->arp_sha[4], arp->arp_sha[5]);

   
        printf("    Target IP  : %d.%d.%d.%d\n",
           arp->arp_tpa[0], arp->arp_tpa[1], arp->arp_tpa[2], arp->arp_tpa[3]);
           
        printf("    Target MAC : %02x:%02x:%02x:%02x:%02x:%02x\n",
           arp->arp_tha[0], arp->arp_tha[1], arp->arp_tha[2],
           arp->arp_tha[3], arp->arp_tha[4], arp->arp_tha[5]);

    }

}

