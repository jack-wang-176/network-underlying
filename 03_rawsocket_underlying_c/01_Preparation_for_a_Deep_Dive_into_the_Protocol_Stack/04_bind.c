#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<net/if.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/ioctl.h>
#include<netinet/ether.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>

int main(int argc, char *argv){
    int raw_socket;
    if((raw_socket = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL)))<0){
        perror("fail to create socket");
        exit(1);
    }
    int eth0_index;
    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,"eth0",IF_NAMESIZE -1);
    if((ioctl(raw_socket,SIOCGIFINDEX,&ifr))<0){
        perror("fail to get id of interface");
        exit(1);
    }
    eth0_index = ifr.ifr_ifindex;
    struct sockaddr_ll sll;
    memset(&sll,0,sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = eth0_index;
    if((bind(raw_socket,(struct sockaddr *)&sll,sizeof(sll)))<0){
        perror("fail to bind");
        exit(1);
    }
    if((ioctl(raw_socket,SIOCGIFFLAGS,&ifr))<0){
        perror("fail to get interface flag");
        exit(1);
    }
    ifr.ifr_flags |= IFF_PROMISC;
    if((ioctl(raw_socket,SIOCSIFFLAGS))<0){
        perror("fail to set interface flag");
        exit(1);
    }
    close(raw_socket);
    return 0;
}
