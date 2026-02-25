#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include <arpa/inet.h>
#include<string.h>

int main(int argc, char const* argv[]){ 
     if(argc <3){
        fprintf(stderr,"Usage %s <IP><Port>",argv[0]);
        exit(1);
     }

     int sockfd;
     if((sockfd = socket(AF_INET,SOCK_STREAM,0))<0){
        perror("fail to create socket");
        exit(1);
     }

     struct sockaddr_in server;
     memset(&server,0,sizeof(server));
     server.sin_family = AF_INET;
     server.sin_port = htons(atoi(argv[2]));
     server.sin_addr.s_addr = inet_addr(argv[1]);
     socklen_t addr = sizeof(server);

     if((connect(sockfd,(struct sockaddr*)&server,addr))<0){
        perror("fail to connet");
        exit(1);
     }
     
     char buf[128]= "";
     char recv[128]= "";
     while(1){
        fgets(buf,sizeof(buf),stdin);
        buf[strlen(buf)-1] = '\0';
        if((send(sockfd,buf,strlen(buf),0))<0){
            perror("fail to send");
            exit(1);
        }
        memset(buf,0,sizeof(buf));
     }
     close(sockfd);
     return 0;
}