#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<errno.h>
#include<fcntl.h>

void unblock(int sock){
    int flag = fcntl(sock,F_GETFL,0);
    if(flag <0){
        perror("fail to unblok");
        exit(1);
    }
    if((fcntl(sock,F_SETFL,flag | O_NONBLOCK))<0){
       perror("fail to unblok");
       exit(1);   
    }
}

int main(int argc,char const* argv[]){
    if(argc<3){
        fprintf(stderr,"Usage %s <IP><Port>",argv[0]);
        exit(1);
    }

    int sockfd;
    if((sockfd = socket(AF_INET,SOCK_STREAM,0))<0){
        perror("fail to build socket");
        exit(1);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(atoi(argv[2]));
    socklen_t addr_cl= sizeof(server);

    if((bind(sockfd,(struct sockaddr*)&server,addr_cl))<0){
        perror("fail to bind");
        exit(1);
    }

    if((listen(sockfd,5))<0){
        perror("fail to listen");
        exit(1);
    }

    int new_sock;
    struct sockaddr_in client;
    char buf[128]="";
    unblock(sockfd);
    while(1){
        socklen_t addr_cl = sizeof(client);
        new_sock = accept(sockfd,(struct sockaddr*)&client,&addr_cl);
        if (new_sock < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK){
                usleep(100000);
                continue;
            }
            else{
                perror("fail to accept");
                exit(1);
            }
        } 
        while(1){
           int num =recv(new_sock,buf,sizeof(buf)-1,0);
           if(num==0){
            printf("client disconneted\n");
            break;
           } else if(num <0){
            perror("fail to recv");
            break;
           }
           buf[num] = '\0';
           printf("%s\n",buf);
           if((send(new_sock,buf,num,0))<0){
            perror("fail to send");
            break;
           }
        }
        close(new_sock);

    }




    close(sockfd);
    return 0;
}