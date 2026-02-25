#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<string.h>

int main(int argc, char const*argv[]){
    if(argc<3){
        fprintf(stderr,"Usage %s <IP><Port>",argv[0]);
        exit(1);
    }

    int sockfd;
    if((sockfd = socket(AF_INET,SOCK_DGRAM,0))<0){
        perror("fail to build socket");
        exit(1);
    }

    struct sockaddr_in meq;
    memset(&meq,0,sizeof(meq));
    meq.sin_addr.s_addr = inet_addr(argv[1]);//224 to 239
    meq.sin_family = AF_INET;
    meq.sin_port = htons(atoi(argv[2]));
    socklen_t addr = sizeof(meq);

    char buf[128] = "";
    while(1){
        fgets(buf,sizeof(buf),stdin);
        if((sendto(sockfd,buf,strlen(buf),0,(struct sockaddr*)&meq,addr))<0){
            perror("fail to send");
            continue;
        }
    }
    
    close(sockfd);
    return 0;
}