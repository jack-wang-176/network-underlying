#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/ether.h> //ETH_P_ALL
#include<unisted.h>
#include<stdlib.h>
#include<stdio.h>
#include<net/if.h>
#include<sys/ioctl.h>
#include<string.h>

int main(int argc,char *argv[]){
    int raw_sockfd;
    if((raw_sockfd = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL))<0)){
        perror("fail to create raw socket");
        exit(1);
    }
    printf("raw_sockfd is %d\n",raw_sockfd);
    struct ifreq ifr;
    strncpy(ifr.ifr_name,"eth0",IFNAMSIZ-1);
    if(ioctl(raw_sockfd,SIOCGIFFLAGS,&ifr)<0){
        perror("fail to ioctl");
        exit(1);
    }
    ifr.ifr_flags |= IFF_PROMISC;
    if(ioctl(raw_sockfd,SIOCSIFFLAGS,&ifr)<0){
        perror("fail to ioctl");
        exit(1);
    }
    printf("set eth0 to promiscuous mode successfully\n");
    close(raw_sockfd);
    return 0;
}