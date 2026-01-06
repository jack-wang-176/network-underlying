#include <stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>//close
#include<string.h>

int main(int argc,char*argv[]){
    //检查是否输入合格
    if(argc < 3){
        fprintf(stderr,"Usage : %s <ServerIP> <Port>\n", argv[0]);
        exit(1);
    }
    int sockfd;
    sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if (sockfd == -1){
        perror("fail to socket");
        exit(1);
    }
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
    serveraddr.sin_port =htons(atoi(argv[2]));
    int addrlen = sizeof(serveraddr);
    
    char buf[32] = "";
    char recv_buf[32]= "";
    //向server上发送，从server上接受
    //所以都是serveraddr结构体
    while(1){
        if(fgets(buf,sizeof(buf),stdin)== NULL)break;
        buf[strlen(buf)-1]= '\0';
        if(sendto(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&serveraddr,addrlen)==-1){
            perror("fail to sendto");
            continue;
        }
        if(recvfrom(sockfd,recv_buf,sizeof(recv_buf),0,(struct sockaddr*)&serveraddr,&addrlen)==-1){
            perror("fail to send to");
            continue;
        }
        printf("Server reply: %s\n", recv_buf);
        // 清空接收缓冲区，防止下次打印乱码
        memset(recv_buf, 0, sizeof(recv_buf));
    }
    close(sockfd);

    return 0;
}