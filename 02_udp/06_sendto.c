#include<stdio.h>
#include<stdlib.h>//exit
#include<sys/types.h>
#include<sys/socket.h>//socket
#include<netinet/in.h>//sockaddr_in
#include<arpa/inet.h>//htons inet_addr
#include<unistd.h>//close
#include<string.h>

#define N 256

int main(int argc,char const *argv[]){
    if(argc<3){
        printf("请输入IP地址和端口号");
        exit(1);
    }
    //第一步创建套接字
    int sockfd;
    if((sockfd = socket(AF_INET,SOCK_DGRAM,0))==-1){
            perror("fail to socket");
            exit(1);
        }
    printf("socke = %d\n",sockfd);
    //第二部填充服务器网络信息结构体 sockaddr_in

    struct sockaddr_in  serveraddr;
    //这里主要是 人去写入这个既定结构体。
    socklen_t addrlen = sizeof(serveraddr);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
    serveraddr.sin_port = htons(atoi(argv[2]));
    //第三步发送数据
    char buf[N] = "";
    while(1)
    {
        //这里fget是标准输入流。
        //n是最大的字符限制
        fgets(buf,N,stdin);
        //这里serveraddr是我们平常常用的
        //但是sockaddr是一个既定程序的接口
        //这里其实进行的是一个压缩
        //完成了这人的看和主机的看
        if(sendto(sockfd,buf,N,0,(struct sockaddr*)&serveraddr,addrlen) == -1){
            perror("faile to sendto data");
            exit(1);
        }
    }
    return 0;

}