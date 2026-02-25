/* 必须放在最第一行 */
#define _GNU_SOURCE 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 网络编程核心头文件 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>  // struct ip_mreq 在这里
#include <arpa/inet.h>

int main(int argc ,char const*argv[]){
    if (argc<3){
        fprintf(stderr,"Usage %s<IP><Port>",argv[0]);
        exit(1);
    }

    int sockfd;
    if((sockfd=socket(AF_INET,SOCK_DGRAM,0))<0){
        perror("fail to create sock");
        exit(1);
    }

    struct ip_mreq mreq;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    mreq.imr_multiaddr.s_addr = inet_addr(argv[1]);
    if((setsockopt(sockfd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)))<0){
        perror("fail to set sock option");
        exit(1);
    }

    
    struct sockaddr_in local_addr; 
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    
    // 修改 3: 绑定到 INADDR_ANY 以确保能收到数据
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    local_addr.sin_port = htons(atoi(argv[2]));
    socklen_t addr_len = sizeof(local_addr);

    if((bind(sockfd,(struct sockaddr*)&local_addr,addr_len))<0){
        perror("fail to bind");
        exit(1);
    }
    
    char buf[128]= "";
    struct sockaddr_in send;
    socklen_t addr = sizeof(send);
    while(1){
        memset(buf,0,sizeof(buf));
        if((recvfrom(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&send,&addr))<0){
            perror("fail to send");
            continue;
        }
        printf("%s from %d %d", buf, send.sin_addr.s_addr, send.sin_port);
    }
    close(sockfd);
    return 0;
}