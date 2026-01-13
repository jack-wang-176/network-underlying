#include<stdio.h>
#include<stdlib.h>//exit
#include<sys/types.h>
#include<sys/socket.h>//socket
#include<netinet/in.h>//sockaddr_in
#include<arpa/inet.h>//htons inet_addr
#include<unistd.h>//close
#include<string.h>

int main(int argc, char const* argv[]){
    if (argc <3){
        fprintf(stderr,"Usage: %s ip port\n",argv[0]);
    }
    // establish socket
    int sockfd;
    if((sockfd = socket(AF_INET,SOCK_DGRAM,0)) == -1)
    {
        perror("fail to socket");
        exit(1);
    }
    // 将服务器的网络信息结构体在绑定前进行填充
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
    serveraddr.sin_port= htons(atoi(argv[2]));

    //将网络信息结构体和套接字绑定
    //这里绑定只是很简单的逻辑，将套接字和网络结构体固定下来
    //这里主要是用在服务器端
    if(bind(sockfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr))== -1)
    {
        perror("fail to bind");
        exit(1);
    }

    return 0;
}