#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<net/if.h>
#include<linux/if_ether.h>
#include<linux/if_packet.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/ioctl.h>
#include<linux/ip.h>
#include<linux/tcp.h>

#define INTERFACE "eth0"
#define SRC_IP "172.31.26.72"
#define DES_IP "172.31.16.1"

//本机地址和目标mac
unsigned char src_mac[6]= {0x00,0x15,0x5d,0x4c,0x7e,0x15};
unsigned char des_mac[6]= {0x00,0x15,0x5d,0xf4,0x2e,0xea};

unsigned short checksum(void *b,int len){
    unsigned int sum;
    unsigned short result;
    unsigned short * flag = b;
    for(sum = 0;len>1;len-=2){
        sum += *flag++;
    }
    if(len == 1){
        sum += *(unsigned char*)flag;
    }
    sum = (sum >> 16)+ (sum & 0xFFFF);
    sum += (sum >>16);
    result = ~sum;
    return result;
}

struct fake_header{
    unsigned int source_address;
    unsigned int dest_address;
    unsigned char placeholder;
    unsigned char protocol;
    unsigned short tcp_length;
} __attribute__((packed));


int main(int argc, char *argv){
    int raw_socket;
    if((raw_socket = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL)))<0){
        perror("fail to create socket");
        exit(1);
    }
    char buf[4096];
    memset(buf,0,sizeof(buf));
    struct ethhdr *eth = (struct ethhdr*)buf;
    struct iphdr *iph = (struct iphdr*)(buf + sizeof( struct ethhdr));
    struct tcphdr *tcp = (struct tcphdr *)(buf + sizeof(struct ethhdr) + sizeof(struct iphdr));
    
    char *data = buf + sizeof(struct ethhdr)+sizeof(struct iphdr) + sizeof(struct tcphdr);
    strcpy(data,"SYN From Raw Socket");
    int data_len = strlen(data);

    memcpy(eth->h_source,src_mac,6);
    memcpy(eth->h_dest,des_mac,6);
    eth -> h_proto = htons(ETH_P_IP);

    iph -> ihl = 5;
    iph -> version = 4;
    iph -> tos = 0;
    iph -> tot_len = htons(sizeof(struct iphdr)+sizeof(struct tcphdr)+data_len);
    iph -> id = htons(54321);
    iph -> frag_off = 0;
    iph -> ttl = 64;
    iph -> protocol = IPPROTO_TCP;
    iph -> saddr = inet_addr(SRC_IP);
    iph -> daddr = inet_addr(DES_IP);
    iph -> check = 0;
    iph -> check = checksum(iph,sizeof(struct iphdr));

    tcp -> source = htons(12345);
    tcp -> dest = htons(80);
    tcp -> seq = htonl(112233);
    tcp -> ack_seq = 0;
    tcp -> syn = 1;
    tcp -> doff = 5;
    tcp -> window = htons(5840);
    tcp -> check = 0;
    struct fake_header ftcp;
    ftcp.source_address = inet_addr(SRC_IP);
    ftcp.dest_address = inet_addr(DES_IP);
    ftcp.placeholder = 0;
    ftcp.protocol = IPPROTO_TCP;
    ftcp.tcp_length = htons(sizeof(struct tcphdr)+ data_len);

    int ctcp = sizeof(struct fake_header)+sizeof(struct tcphdr)+data_len;
    char *cctcp = malloc(ctcp);
    memcpy(cctcp,(char*)&ftcp,sizeof(struct fake_header));
    memcpy(cctcp+ sizeof(struct fake_header),(char*)tcp,sizeof(struct tcphdr)+data_len);
    tcp -> check = checksum(cctcp,ctcp);
    free(cctcp);

    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,INTERFACE,IF_NAMESIZE -1);
    if((ioctl(raw_socket,SIOCGIFINDEX,&ifr))<0){
        perror("fail to bind interface");
        exit(1);
    }

    struct sockaddr_ll sll;
    memset(&sll,0,sizeof(sll));
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_halen = ETH_ALEN;
    sll.sll_family = AF_PACKET;
    int total_len = sizeof(struct ethhdr)+sizeof(struct iphdr)+ sizeof(struct tcphdr)+data_len;
    if((sendto(raw_socket,buf,total_len,0,(struct sockaddr*)&sll,sizeof(sll)))<0){
        perror("fail to sendto\n");
    }else {
        printf("success to sendto\n");
    }
    close(raw_socket);
    return 0;
}