#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>

int main(int argc,char const * argv[]){
    if(argc<3){
        fprintf(stderr,"Usage %s <IP><Port>",argv[0]);
        exit(1);
    }

    int sockfd;
    if((sockfd= socket(AF_INET,SOCK_STREAM,0))<0){
        perror("fail to create socket");
        exit(1);
    }

    struct sockaddr_in server;
    memset(&server,0,sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));
    server.sin_addr.s_addr = inet_addr(argv[1]);
    socklen_t ser_len = sizeof(server);

    if((bind(sockfd,(struct sockaddr*)&server,ser_len))<0){
        perror("fail to bind");
        exit(1);
    }

    if((listen(sockfd,5))<0){
        perror("fail to listen");
        exit(1);
    }

    int new_sockfd;
    struct sockaddr_in client;
    socklen_t clt_len = sizeof(client);
    if((new_sockfd = accept(sockfd,(struct sockaddr*)&client,&clt_len))<0){
        perror("fail to accept");
        exit(1);
    }

    char buf[128] = "";
    int ret;
    while(1){
        ret =recv(new_sockfd,buf,sizeof(buf)-1,0);
        if (ret == 0){
            printf("client close");
            perror("client is close");
            exit(1);
        }
        if(ret <0){
            perror("can not recv");
            exit(1);
        }
        buf[ret]= '\0';
        printf("%s\n",buf);
        memset(buf,0,sizeof(buf));

    }

    close(new_sockfd);
    close(sockfd);
    return 0;
}