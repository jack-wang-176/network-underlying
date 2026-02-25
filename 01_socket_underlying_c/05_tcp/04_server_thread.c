#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<pthread.h>

typedef struct{
    struct sockaddr_in client;
    int clt_socket;
}net_info;

void* communication(void* arg){
    pthread_detach(pthread_self());
    net_info* client_info = (net_info*)arg;
    int sock = client_info->clt_socket;
    struct sockaddr_in client_addr = client_info->client;
    free(arg);
    char buf[128] = "";
    int len = sizeof(buf)-1;
    int flag;
    while(1){
        flag =recv(sock,buf,len,0);
        if (flag == 0){
            printf("client break");
            break;
        }
        else if(flag >0){
            buf[flag]= '\0';
            printf("%s\n",buf);
        if((send(sock,buf,flag,0))<0){
            perror("fail to send");
            break;
        }
        }else {
            perror("fail to recv");
            break;
        }
        memset(buf,0,sizeof(buf));
      
    }
    close(sock);
    return NULL;
}

int main(int argc, char const*argv[]){
    if(argc<3){
        fprintf(stderr,"Usage %s<IP><Port>",argv[0]);
        exit(1);
    }

    int sockfd;
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("fail to build socket");
        exit(1);
    }

    struct sockaddr_in server;
    memset(&server,0,sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(atoi(argv[2]));
    socklen_t sev = sizeof(server);

    if((bind(sockfd,(struct sockaddr*)&server,sev)<0)){
        perror("fail to bind");
        exit(1);
    }

    if((listen(sockfd,5))<0){
        perror("fail to listen"); 
        exit(1);
    }

    pthread_t pid;
    int c_sockfd;
    struct sockaddr_in client;   
    net_info* client_p;
    while(1){
        socklen_t clt_len =  sizeof(client);
        c_sockfd = accept(sockfd,(struct sockaddr*)&client,&clt_len);
        if(c_sockfd<0){
            perror("fail to accept");
            continue;
        }
        client_p =malloc(sizeof(net_info));
        client_p->clt_socket = c_sockfd;
        client_p->client = client;
        pthread_create(&pid,NULL,communication,client_p);
    }
    


    close(sockfd);
    return 0;
}