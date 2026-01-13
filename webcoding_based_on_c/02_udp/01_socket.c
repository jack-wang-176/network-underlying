#include<stdio.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<stdlib.h>

int main(int argc, char const *argv[]){

    int sockfd;
    //这里套接字已经被创建了
    //-1是错误返回
    //返回一个在文件描述符里空闲的整数
    //这个整数代表那个套接字
    if((sockfd = socket(AF_INET,SOCK_DGRAM,0))==-1){
        perror("fail to socket");
        exit(1);
    }

    printf("sockfd = %d\n",sockfd);
    return 0;
}