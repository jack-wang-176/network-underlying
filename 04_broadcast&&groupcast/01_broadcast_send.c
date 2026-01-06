#include<stdio.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<string.h>
#include <unistd.h>

int main(int argc,char* const argv[])
{
    if(argc<3){
        fprintf(stderr,"Usage : %s<IP> <PORT>\n",argv[0]);
        exit(1);
    }
    struct sockaddr_in bc;
    memset(&bc,0,sizeof(bc));
    bc.sin_family = AF_INET;
    bc.sin_addr.s_addr = inet_addr(argv[1]);
    bc.sin_port = htons(atoi(argv[2]));
    socklen_t addrlen = sizeof(bc);

    int sockfd;
    if((sockfd  = socket(AF_INET,SOCK_DGRAM,0))<0){
        perror("fail to create sock");
        exit(1);
    }
//赋予广播发送权限
    int pm = 1;
    if((setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&pm,sizeof(pm)))<0){
        perror("fail to setsock");
        exit(1);
    }

    char buf[128]= "";
    while(1){
        fgets(buf,sizeof(buf),stdin);
        if((sendto(sockfd,buf,strlen(buf),0,(struct sockaddr*)&bc,addrlen))<0){
            perror("fail to send to");
            continue;
        }
    }
    close(sockfd);
    return 0;
}
