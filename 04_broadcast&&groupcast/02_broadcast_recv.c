#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>
#include <unistd.h>

int main(int argc,char* const argv[]){
    if(argc<3){
        fprintf(stderr,"Usage%s <IP><PORT>",argv[0]);
        exit(1);
    }
    
    int sockfd;
    if((sockfd = socket(AF_INET,SOCK_DGRAM,0))<0){
        perror("fail to create socket");
        exit(1);
    }

    struct sockaddr_in bd;
    memset(&bd,0,sizeof(bd));
    bd.sin_family= AF_INET;
    bd.sin_addr.s_addr = inet_addr(argv[1]);
    bd.sin_port = htons(atoi(argv[2]));
    socklen_t addrlen = sizeof(bd);

    if((bind(sockfd,(struct sockaddr*)&bd,addrlen))<0){
        perror("fail to bind");
        exit(1);
    }

    char buf[128]= "";
    while(1){
        memset(buf,0,sizeof(buf));
        if((recvfrom(sockfd,&buf,sizeof(buf),0,(struct sockaddr*)&bd,&addrlen)<0)){
            perror("fail to recv");
            continue;
        }
        printf("%s", buf);
    }
    close(sockfd);
    return 0;

}