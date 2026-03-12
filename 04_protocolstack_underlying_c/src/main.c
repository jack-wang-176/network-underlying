#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>
#include<sys/fcntl.h>
#include<sys/ioctl.h>
#include<linux/if.h>
#include<linux/if_tun.h>
#include<arpa/inet.h>
#include"../include/eth.h"
#include"../include/arp.h"


void handle_arp(int fd,struct eth_hdr *ethd,struct arp_hdr *arpd,unsigned char* mac);
int tun_alloc(char *dev){
    int fd,err;
    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    if((fd = open("/dev/net/tun",O_RDWR))<0){
        perror("fail to open /dev/net/tun");
        return fd;
    }
    ifr.ifr_flags |= IFF_TAP |IFF_NO_PI;
    if(*dev){
        strncpy(ifr.ifr_name,dev,IFNAMSIZ);
    }
    if((err = ioctl(fd,TUNSETIFF,&ifr))<0){
        perror("fail to ioctl tun");
        close(fd);
        return err;
    }
    strcpy(dev,ifr.ifr_name);
    return fd;
}
unsigned char* tun_mac(char *dev,unsigned char* mac){
    int fd;
    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    if((fd = socket(AF_INET,SOCK_DGRAM,0))<0){
        perror("fail to create temporary socket");
        exit(1);
    }
    if((ioctl(fd,SIOCGIFHWADDR,&ifr))<0){
        perror("fail to get interface addr");
        exit(1);
    }
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    close(fd);
    return mac;
}
int main(int argc, char*argv[]){
    char tap_name[IFNAMSIZ] = "tap0";
    unsigned char mymac[6];
    int tap_fd = tun_alloc(tap_name);
    if(tap_fd<0){
        fprintf(stderr, "Error connecting to tap interface %s!\n", tap_name);
        exit(1);
    }
    printf("Successfully attached to %s\n", tap_name);
    printf("Listening for packets...\n\n");

    unsigned char* mac = tun_mac(tap_name,mymac);
    unsigned char buffer[1522];
    while(1){
        ssize_t nread = read(tap_fd,buffer,sizeof(buffer));
        if (nread < 0){
            perror("Read error");
            break;
        } 
        if (nread <sizeof(struct ethhdr)){
            continue;
        }
        printf("Read %zd bytes from %s:\n", nread, tap_name);
        struct eth_hdr *eth = (struct eth_hdr*)buffer;
        uint16_t protocol = ntohs(eth->ethtype);
        printf("  |-源 MAC  : %02x:%02x:%02x:%02x:%02x:%02x\n",
               eth->smac[0], eth->smac[1], eth->smac[2],
               eth->smac[3], eth->smac[4], eth->smac[5]);

        printf("  |-目的 MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               eth->dmac[0], eth->dmac[1], eth->dmac[2],
               eth->dmac[3], eth->dmac[4], eth->dmac[5]);
        printf("  |-协议类型: %04x",protocol);
        switch(protocol){
            case 0x0800:
              printf("  (IP Protocol)\n");
              break;
            case 0x0806:
              printf("  (ARP Protocol)\n");
              struct arp_hdr *arph = (struct arp_hdr*)(buffer + sizeof(struct eth_hdr));
              uint16_t opcode = ntohs(arph -> opcode);
              printf("  |---[ARP 详细信息]---\n");
              printf("      |-操作码: %d (%s)\n", opcode, opcode == ARP_REQUEST ? "请求mac地址" : "应答mac地址");
              struct in_addr sendaddr,recvaddr;
              sendaddr.s_addr = arph -> sip;
              recvaddr.s_addr = arph -> dip;
              printf("      |-发送方 IP : %s\n", inet_ntoa(sendaddr));
              printf("      |-寻找目标 IP: %s\n", inet_ntoa(recvaddr));
              handle_arp(tap_fd,eth,arph,mac);
              break;
            case 0x86dd:
              printf("  (IPv6 Protocol)\n");
              break;
            default:
              printf("  (Other Protocol)\n");
        }
        printf("----------------------------------------\n");

        for (int i = 0; i < nread; i++) {
            printf("%02x ", buffer[i]);
            if ((i + 1) % 16 == 0)
            printf("\n"); 
        }
        printf("\n\n");
    }
    close(tap_fd);
    return 0;
}