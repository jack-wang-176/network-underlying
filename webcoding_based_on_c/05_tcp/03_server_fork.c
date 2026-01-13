#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

void handler(int sig){
    while((waitpid(-1,NULL,WNOHANG))>0){}
}

int main(int argc, char const*argv[]){
    if(argc<3){
        fprintf(stderr,"Usage: %s<IP><Port>",argv[0]);
        exit(1);
    }

    int sockfd;
    if((sockfd = socket(AF_INET,SOCK_STREAM,0))<0){
        perror("fail to creat socket");
        exit(1);
    }

    struct sockaddr_in server;
    memset(&server,0,sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(atoi(argv[2]));
    socklen_t addr = sizeof(server);

    if((bind(sockfd,(struct sockaddr*)&server,addr))<0){
        perror("fail to bind");
        exit(1);
    }

    if((listen(sockfd,5))<0){
        perror("fail to listen");
        exit(1);
    }
//这里实际上是通过pidwait将一个阻塞型的io转化为非阻塞型
//只要一旦出现死亡就一定去回收。
    signal(SIGCHLD,handler);
    
    int new_socket;
    struct sockaddr_in client;
    socklen_t addr_cl = sizeof(client);
    pid_t pid;
    char buf[128] = "";
    int msg_len;
    while(1){
       new_socket = accept(sockfd,(struct sockaddr *)&client,&addr_cl);
       if(new_socket<0){
            if (errno == EINTR){
                continue;
            }
            else{
                perror("fail to exit");
                exit(1);
            }
       } 
       pid = fork();
       if(pid <0){
            perror("fail to fork");
            close(new_socket);
            exit(1);
       }
       //父进程
       else if(pid>0){
        close(new_socket);
       }
       else{
        close(sockfd);
        signal(SIGCHLD,SIG_IGN);
        while(1){    
            msg_len =recv(new_socket,buf,sizeof(buf),0);
            if (msg_len == 0){
                printf("client closed %s",inet_ntoa(client.sin_addr));
                exit(0);
            }
            if(msg_len<0){
                perror("fail to recv");
                break;
            }else{
                printf("recv %s\n",buf);
                send(new_socket,buf,strlen(buf),0);
                memset(buf,0,sizeof(buf));
            }
           
        }
        close(new_socket);
       }
    }

    if(sockfd>0){
        close(sockfd);
    }
    return 0;
}